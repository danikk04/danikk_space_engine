#include <algorithm>
#include <cmath>

#include <danikk_space_engine/profiler.h>
#include <danikk_space_engine/component/world_body/planet/chunk_generator.h>
#include <danikk_space_engine/component/world_body/planet/chunk_save.h>

namespace danikk_space_engine
{
	namespace
	{
		constexpr uint64 empty_edge_key = static_cast<uint64>(-1);

		template<typename value_t>
		/**
		 * @brief Exchanges two values using move construction and move assignment.
		 * @tparam value_t Value type supporting move construction and assignment.
		 * @param left First value to exchange.
		 * @param right Second value to exchange.
		 */
		void swapValues(value_t& left, value_t& right)
		{
			value_t temporary = static_cast<value_t&&>(left);
			left = static_cast<value_t&&>(right);
			right = static_cast<value_t&&>(temporary);
		}

		/**
		 * @brief Holds a scalar-field sample on the chunk's regular 3D lattice.
		 * `id` is the flattened lattice index used to form shared edge-cache keys;
		 * `position` is planet-local and `density` is positive inside solid terrain.
		 */
		struct GridPoint
		{
			uint32 id;
			glm::dvec3 position;
			float density;
		};

		/**
		 * @brief Returns the smallest power of two not less than the requested capacity.
		 * @param value Requested minimum capacity; zero returns one.
		 * @return Power-of-two size used by the open-addressed edge-intersection cache.
		 */
		size_t nextPowerOfTwo(size_t value)
		{
			size_t result = 1;
			while(result < value) result <<= 1;
			return result;
		}

		/**
		 * @brief Packs an unordered pair of 32-bit lattice vertex IDs into a 64-bit key.
		 * @param a First endpoint ID.
		 * @param b Second endpoint ID.
		 * @return Same key for either endpoint order, allowing neighboring tetrahedra
		 *         to reuse a single interpolated surface vertex.
		 */
		uint64 edgeKey(uint32 a, uint32 b)
		{
			if(a > b) swapValues(a, b);
			return (static_cast<uint64>(a) << 32) | b;
		}

		/**
		 * @brief Samples and polygonizes one regular 3D chunk of the planetary scalar field.
		 * @param request Immutable planet parameters, terrain edits, grid coordinate,
		 *                and chunk origin captured when the worker job was submitted.
		 * @return Request identity plus chunk-local surface vertices and triangle indices.
		 * The field is sampled at every lattice corner. Each lattice cube is split
		 * into six tetrahedra; sign-changing tetrahedron edges are linearly
		 * interpolated and cached by an unordered endpoint key so adjacent
		 * tetrahedra share vertices. The resulting 3- or 4-vertex polygon is angle
		 * sorted, fan-triangulated, and wound from solid toward air. This helper
		 * reads no WorldData and creates no GPU resources.
		 */
		GeneratedChunkData generatePlanetChunkGeometry(const ChunkGenerationRequest& request)
		{
			GeneratedChunkData result;
			result.generation_id = request.generation_id;
			result.world_body_id = request.world_body_id;
			result.component_slot_id = request.component_slot_id;
			result.component_generation_id = request.component_generation_id;
			result.terrain_revision = request.terrain_revision;
			result.grid_coordinate = request.grid_coordinate;
			result.chunk_position = request.chunk_position;

			const size_t cells_per_axis = static_cast<size_t>(std::ceil(request.chunk_width / request.voxel_size));
			const size_t points_per_axis = cells_per_axis + 1;
			const size_t lattice_point_count = points_per_axis * points_per_axis * points_per_axis;
			const auto pointId = [points_per_axis](size_t x, size_t y, size_t z)
			{
				return static_cast<uint32>((z * points_per_axis + y) * points_per_axis + x);
			};
			const auto pointPosition = [&request](size_t x, size_t y, size_t z)
			{
				return request.chunk_position + glm::dvec3(
					static_cast<double>(x) * request.voxel_size,
					static_cast<double>(y) * request.voxel_size,
					static_cast<double>(z) * request.voxel_size);
			};
			DynamicArray<double> densities;
			densities.resize(lattice_point_count);
			for(size_t z = 0; z < points_per_axis; z++)
			{
				for(size_t y = 0; y < points_per_axis; y++)
				{
					for(size_t x = 0; x < points_per_axis; x++)
					{
						const size_t point_index = pointId(x, y, z);
						densities[point_index] = samplePlanetScalarField(
							pointPosition(x, y, z), request.planet_radius, request.noise_seed,
							request.terrain_edits, request.core_radius);
					}
				}
			}

			const size_t cell_count = cells_per_axis * cells_per_axis * cells_per_axis;
			const size_t edge_cache_size = nextPowerOfTwo(std::max<size_t>(64, cell_count * 64));
			DynamicArray<uint64> edge_keys;
			edge_keys.resize(edge_cache_size, empty_edge_key);
			DynamicArray<uint32> edge_values;
			edge_values.resize(edge_cache_size, 0);
			const size_t edge_cache_mask = edge_cache_size - 1;
			const auto getEdgeVertex = [&](const GridPoint& first, const GridPoint& second)
			{
				const uint64 key = edgeKey(first.id, second.id);
				uint64 hash = key;
				hash ^= hash >> 33;
				hash *= 0xff51afd7ed558ccdULL;
				hash ^= hash >> 33;
				hash *= 0xc4ceb9fe1a85ec53ULL;
				hash ^= hash >> 33;
				size_t slot_index = static_cast<size_t>(hash) & edge_cache_mask;
				while(edge_keys[slot_index] != empty_edge_key && edge_keys[slot_index] != key)
				{
					slot_index = (slot_index + 1) & edge_cache_mask;
				}
				if(edge_keys[slot_index] == key) return edge_values[slot_index];

				const double denominator = static_cast<double>(first.density) - second.density;
				const double interpolation = std::abs(denominator) < 0.000001
					? 0.5
					: glm::clamp(static_cast<double>(first.density) / denominator, 0.0, 1.0);
				const glm::dvec3 position = glm::mix(first.position, second.position, interpolation);
				const uint32 vertex_index = static_cast<uint32>(result.point_positions.size());
				result.point_positions.push(vec3(position - request.chunk_position));
				edge_keys[slot_index] = key;
				edge_values[slot_index] = vertex_index;
				return vertex_index;
			};

			constexpr uint8 cube_corner_offsets[8][3]{
				{0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0},
				{0, 0, 1}, {1, 0, 1}, {1, 1, 1}, {0, 1, 1}};
			constexpr uint8 tetrahedra[6][4]{
				{0, 5, 1, 6}, {0, 1, 2, 6}, {0, 2, 3, 6},
				{0, 3, 7, 6}, {0, 7, 4, 6}, {0, 4, 5, 6}};
			constexpr uint8 tetra_edges[6][2]{{0, 1}, {0, 2}, {0, 3}, {1, 2}, {1, 3}, {2, 3}};
			result.point_positions.reserve(cell_count * 8);
			result.triangle_point_indices.reserve(cell_count * 12);

			for(size_t z = 0; z < cells_per_axis; z++)
			{
				for(size_t y = 0; y < cells_per_axis; y++)
				{
					for(size_t x = 0; x < cells_per_axis; x++)
					{
						GridPoint cube_points[8];
						for(uint8 corner = 0; corner < 8; corner++)
						{
							const size_t px = x + cube_corner_offsets[corner][0];
							const size_t py = y + cube_corner_offsets[corner][1];
							const size_t pz = z + cube_corner_offsets[corner][2];
							const uint32 id = pointId(px, py, pz);
							cube_points[corner] = {id, pointPosition(px, py, pz), static_cast<float>(densities[id])};
						}

						for(const auto& tetrahedron : tetrahedra)
						{
							GridPoint tetra_points[4];
							glm::dvec3 inside_center(0.0);
							glm::dvec3 outside_center(0.0);
							uint8 inside_count = 0;
							uint8 outside_count = 0;
							for(uint8 index = 0; index < 4; index++)
							{
								tetra_points[index] = cube_points[tetrahedron[index]];
								if(tetra_points[index].density >= 0.0f)
								{
									inside_center += tetra_points[index].position;
									inside_count++;
								}
								else
								{
									outside_center += tetra_points[index].position;
									outside_count++;
								}
							}
							if(inside_count == 0 || outside_count == 0) continue;
							inside_center /= static_cast<double>(inside_count);
							outside_center /= static_cast<double>(outside_count);
							const glm::dvec3 outward = outside_center - inside_center;

							uint32 polygon[4]{};
							uint8 polygon_size = 0;
							for(const auto& edge : tetra_edges)
							{
								const GridPoint& a = tetra_points[edge[0]];
								const GridPoint& b = tetra_points[edge[1]];
								if((a.density >= 0.0f) == (b.density >= 0.0f)) continue;
								polygon[polygon_size++] = getEdgeVertex(a, b);
							}
							if(polygon_size < 3) continue;

							glm::dvec3 center(0.0);
							for(uint8 index = 0; index < polygon_size; index++)
							{
								center += glm::dvec3(result.point_positions[polygon[index]]) + request.chunk_position;
							}
							center /= static_cast<double>(polygon_size);
							const glm::dvec3 normal = glm::normalize(outward);
							glm::dvec3 axis_u = glm::cross(
								normal, glm::dvec3(result.point_positions[polygon[0]]) + request.chunk_position - center);
							if(glm::dot(axis_u, axis_u) < 0.000000000001)
							{
								const glm::dvec3 fallback_axis = std::abs(normal.x) < 0.9
									? glm::dvec3(1.0, 0.0, 0.0)
									: glm::dvec3(0.0, 1.0, 0.0);
								axis_u = glm::cross(normal, fallback_axis);
							}
							axis_u = glm::normalize(axis_u);
							const glm::dvec3 axis_v = glm::cross(normal, axis_u);
							for(uint8 left = 0; left < polygon_size; left++)
							{
								for(uint8 right = left + 1; right < polygon_size; right++)
								{
									const glm::dvec3 left_point = glm::dvec3(result.point_positions[polygon[left]])
										+ request.chunk_position - center;
									const glm::dvec3 right_point = glm::dvec3(result.point_positions[polygon[right]])
										+ request.chunk_position - center;
									const double left_angle = std::atan2(glm::dot(left_point, axis_v), glm::dot(left_point, axis_u));
									const double right_angle = std::atan2(glm::dot(right_point, axis_v), glm::dot(right_point, axis_u));
									if(left_angle > right_angle) swapValues(polygon[left], polygon[right]);
								}
							}

							for(uint8 triangle = 1; triangle + 1 < polygon_size; triangle++)
							{
								uint32 a = polygon[0];
								uint32 b = polygon[triangle];
								uint32 c = polygon[triangle + 1];
								const vec3 pa = result.point_positions[a];
								const vec3 pb = result.point_positions[b];
								const vec3 pc = result.point_positions[c];
								if(glm::dot(glm::cross(pb - pa, pc - pa), vec3(outward)) < 0.0f) swapValues(b, c);
								result.triangle_point_indices.push(a);
								result.triangle_point_indices.push(b);
								result.triangle_point_indices.push(c);
							}
						}
					}
				}
			}
			return result;
		}
	}

	/**
	 * @brief Loads compatible saved geometry or generates, saves, and materializes a planet chunk result.
	 * @param request Immutable generation identity, settings, edits, and chunk coordinate.
	 * @return Worker result containing request identity, local geometry, and a
	 *         material ID for each triangle; geometry arrays may be empty when the
	 *         sampled field contains no surface in this chunk.
	 * Material IDs are recalculated from each triangle centroid after either load
	 * or generation, so material assignment is not part of the mesh save format.
	 * This worker-side function does not allocate WorldData resources or upload a mesh.
	 */
	GeneratedChunkData generatePlanetChunk(const ChunkGenerationRequest& request)
	{
		/**
		 * @brief Measures the duration of the enclosing profiler scope.
		 */
		DSE_WATCH();
		GeneratedChunkData result;

		if(!loadPlanetChunk(request, result))
		{
			result = generatePlanetChunkGeometry(request);
			savePlanetChunk(request, result);
		}

		const size_t triangle_count = result.triangle_point_indices.size() / 3;
		result.triangle_material_ids.reserve(triangle_count);
		for(size_t triangle = 0; triangle < triangle_count; triangle++)
		{
			const size_t first_index = triangle * 3;
			const vec3 a = result.point_positions[result.triangle_point_indices[first_index]];
			const vec3 b = result.point_positions[result.triangle_point_indices[first_index + 1]];
			const vec3 c = result.point_positions[result.triangle_point_indices[first_index + 2]];
			const glm::dvec3 center = request.chunk_position
				+ (glm::dvec3(a) + glm::dvec3(b) + glm::dvec3(c)) / 3.0;
			result.triangle_material_ids.push(getPlanetSurfaceMaterial(
				center, request.planet_radius, request.noise_seed));
		}
		return result;
	}
}
