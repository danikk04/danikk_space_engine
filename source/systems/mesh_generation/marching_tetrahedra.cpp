#include <algorithm>
#include <cmath>

#include <danikk_space_engine/systems/telemetry/telemetry.h>
#include <danikk_space_engine/world.h>
#include <danikk_space_engine/systems/mesh_generation/mesh_generation.h>
#include "internal.h"

#include <danikk_engine/danikk_engine.h>

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
		 * @brief Holds a scalar-field sample on the terrain node's regular 3D lattice.
		 * `id` is the flattened lattice index used to form shared edge-cache keys;
		 * `position` is planet-local and `density` is positive inside solid terrain.
		 */
		struct GridPoint
		{
			uint32 id;
			glm::dvec3 position;
			double density;
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

	}

	namespace mesh_generation_internal
	{
		/**
		 * @brief Samples and polygonizes one regular 3D terrain-tree node.
		 * @param world World providing live planet terrain data.
		 * @param job Node bounds, origin, generator settings, and output arrays.
		 * The field is sampled at every lattice corner. Each lattice cube is split
		 * into six tetrahedra; sign-changing tetrahedron edges are refined against
		 * the original scalar field by bisection and cached by an unordered endpoint key so adjacent
		 * tetrahedra share vertices. Tetrahedron cases explicitly connect their
		 * three- or four-vertex intersections, and triangles are wound from solid
		 * toward air. Terrain samples are read directly from World; this helper creates
		 * no GPU resources.
		 */
		void generatePlanetTerrainNodeMarchingTetrahedra(World& world, MeshGenerationJob& job)
		{
			const size_t cells_per_axis = static_cast<size_t>(std::ceil(job.node_width / job.voxel_size));
			const size_t points_per_axis = cells_per_axis + 1;
			const size_t lattice_point_count = points_per_axis * points_per_axis * points_per_axis;
			const auto pointId = [points_per_axis](size_t x, size_t y, size_t z)
			{
				return static_cast<uint32>((z * points_per_axis + y) * points_per_axis + x);
			};
			const auto pointPosition = [&job](size_t x, size_t y, size_t z)
			{
				return job.node_minimum + glm::dvec3(
					static_cast<double>(x) * job.voxel_size,
					static_cast<double>(y) * job.voxel_size,
					static_cast<double>(z) * job.voxel_size);
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
						densities[point_index] = world.planets.getPlanetDensity(
							job.planet_id, pointPosition(x, y, z),
							job.surface_only
								? PlanetTerrainMode::heightmap_with_edits
								: PlanetTerrainMode::cave_field_with_edits);
					}
				}
			}

			const size_t cell_count = cells_per_axis * cells_per_axis * cells_per_axis;
		// The tetrahedral split introduces fewer than 20 distinct edges per cell.
		// Keep the open-addressed table below a roughly 60% load instead of reserving 64 slots per cell.
		const size_t edge_cache_size = nextPowerOfTwo(std::max<size_t>(64, cell_count * 32));
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

				// Start with linear interpolation between the cached lattice samples.
				const double density_delta = first.density - second.density;
				const double interpolation = std::clamp(
					std::abs(density_delta) > 1e-12 ? first.density / density_delta : 0.5,
					0.0, 1.0);
				glm::dvec3 position = glm::mix(first.position, second.position, interpolation);
				if(world.planets.getPlanetDensity(job.planet_id, position,
					job.surface_only ? PlanetTerrainMode::heightmap_with_edits
						: PlanetTerrainMode::cave_field_with_edits) < 0.0)
				{
					glm::dvec3 inside = first.density >= 0.0 ? first.position : second.position;
					glm::dvec3 outside = position;
					for(uint8 iteration = 0; iteration < 6; ++iteration)
					{
						const glm::dvec3 midpoint = (inside + outside) * 0.5;
						if(world.planets.getPlanetDensity(job.planet_id, midpoint,
							job.surface_only ? PlanetTerrainMode::heightmap_with_edits
								: PlanetTerrainMode::cave_field_with_edits) >= 0.0)
							inside = midpoint;
						else
							outside = midpoint;
					}
					position = inside;
				}
				const uint32 vertex_index = static_cast<uint32>(job.point_positions.size());
				job.point_positions.push(vec3(position - job.node_minimum));
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
			job.point_positions.reserve(cell_count * 8);
			job.triangle_point_indices.reserve(cell_count * 12);
			const auto appendTriangle = [&job](uint32 a, uint32 b, uint32 c, const glm::dvec3& outward)
			{
				const vec3 pa = job.point_positions[a];
				const vec3 pb = job.point_positions[b];
				const vec3 pc = job.point_positions[c];
				if(glm::dot(glm::cross(pb - pa, pc - pa), vec3(outward)) < 0.0f) swapValues(b, c);
				job.triangle_point_indices.push(a);
				job.triangle_point_indices.push(b);
				job.triangle_point_indices.push(c);
			};

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
							cube_points[corner] = {id, pointPosition(px, py, pz), densities[id]};
						}

						for(const auto& tetrahedron : tetrahedra)
						{
							GridPoint tetra_points[4];
							GridPoint inside_points[3];
							GridPoint outside_points[3];
							glm::dvec3 inside_center(0.0);
							glm::dvec3 outside_center(0.0);
							uint8 inside_count = 0;
							uint8 outside_count = 0;
							for(uint8 index = 0; index < 4; index++)
							{
								tetra_points[index] = cube_points[tetrahedron[index]];
								if(tetra_points[index].density >= 0.0f)
								{
									inside_points[inside_count] = tetra_points[index];
									inside_center += tetra_points[index].position;
									inside_count++;
								}
								else
								{
									outside_points[outside_count] = tetra_points[index];
									outside_center += tetra_points[index].position;
									outside_count++;
								}
							}
							if(inside_count == 0 || outside_count == 0) continue;
							inside_center /= static_cast<double>(inside_count);
							outside_center /= static_cast<double>(outside_count);
							const glm::dvec3 outward = outside_center - inside_center;
							if(inside_count == 1)
							{
								appendTriangle(
									getEdgeVertex(inside_points[0], outside_points[0]),
									getEdgeVertex(inside_points[0], outside_points[1]),
									getEdgeVertex(inside_points[0], outside_points[2]), outward);
							}
							else if(inside_count == 3)
							{
								appendTriangle(
									getEdgeVertex(outside_points[0], inside_points[0]),
									getEdgeVertex(outside_points[0], inside_points[1]),
									getEdgeVertex(outside_points[0], inside_points[2]), outward);
							}
							else
							{
								const uint32 inside_outside_00 = getEdgeVertex(inside_points[0], outside_points[0]);
								const uint32 inside_outside_01 = getEdgeVertex(inside_points[0], outside_points[1]);
								const uint32 inside_outside_10 = getEdgeVertex(inside_points[1], outside_points[0]);
								const uint32 inside_outside_11 = getEdgeVertex(inside_points[1], outside_points[1]);
								appendTriangle(inside_outside_00, inside_outside_01, inside_outside_11, outward);
								appendTriangle(inside_outside_00, inside_outside_11, inside_outside_10, outward);
							}
						}
					}
				}
			}
			return;
		}
	}
}
