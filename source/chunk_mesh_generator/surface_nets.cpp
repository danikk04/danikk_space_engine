#include <algorithm>
#include <cmath>

#include <danikk_space_engine/chunk_mesh_generator.h>
#include <danikk_space_engine/planet_terrain_system.h>

namespace danikk_space_engine::chunk_mesh_generator_internal
{
	/**
	 * @brief Extracts a planet isosurface with the Surface Nets algorithm.
	 * @param request Immutable field snapshot, chunk coordinate, and generator settings.
	 * @return Chunk-local vertices and triangle indices, including halo-cell vertices needed
	 *         to connect faces across chunk boundaries.
	 * The scalar field is sampled on a regular lattice expanded by one cell around the
	 * chunk. Each sign-changing cell receives one vertex at the average of its edge
	 * intersections. Every sign-changing lattice edge then connects the four adjacent
	 * cell vertices into a quad, split along its shorter diagonal. Edge ownership uses
	 * the half-open chunk lattice range, so neighboring chunks emit each quad once.
	 */
	GeneratedChunkMesh generatePlanetTerrainChunkMeshSurfaceNets(const ChunkMeshGenerationRequest& request)
	{
		GeneratedChunkMesh result;
		result.generation_id = request.generation_id;
		result.world_body_id = request.world_body_id;
		result.component_slot_id = request.component_slot_id;
		result.component_generation_id = request.component_generation_id;
		result.terrain_revision = request.terrain_revision;
		result.source_revision = request.source_revision;
		result.source = request.source;
		result.planet_mesh_generator = request.planet_mesh_generator;
		result.grid_coordinate = request.grid_coordinate;
		result.chunk_position = request.chunk_position;

		const size_t cells_per_axis = static_cast<size_t>(std::ceil(request.chunk_width / request.voxel_size));
		const size_t sample_side = cells_per_axis + 3;
		const size_t cell_side = cells_per_axis + 1;
		const auto sampleIndex = [sample_side](int x, int y, int z)
		{
			return (static_cast<size_t>(z + 1) * sample_side + static_cast<size_t>(y + 1)) * sample_side
				+ static_cast<size_t>(x + 1);
		};
		const auto cellIndex = [cell_side](int x, int y, int z)
		{
			return (static_cast<size_t>(z + 1) * cell_side + static_cast<size_t>(y + 1)) * cell_side
				+ static_cast<size_t>(x + 1);
		};
		const auto samplePosition = [&request](int x, int y, int z)
		{
			return request.chunk_position + glm::dvec3(
				static_cast<double>(x) * request.voxel_size,
				static_cast<double>(y) * request.voxel_size,
				static_cast<double>(z) * request.voxel_size);
		};

		DynamicArray<double> densities;
		densities.resize(sample_side * sample_side * sample_side);
		for(int z = -1; z <= static_cast<int>(cells_per_axis) + 1; ++z)
			for(int y = -1; y <= static_cast<int>(cells_per_axis) + 1; ++y)
				for(int x = -1; x <= static_cast<int>(cells_per_axis) + 1; ++x)
					densities[sampleIndex(x, y, z)] = PlanetTerrainSystem::getPlanetDensity(
						*request.planet_terrain, samplePosition(x, y, z));

		constexpr uint8 corner_offsets[8][3]{
			{0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0},
			{0, 0, 1}, {1, 0, 1}, {1, 1, 1}, {0, 1, 1}};
		constexpr uint8 cube_edges[12][2]{
			{0, 1}, {1, 2}, {2, 3}, {3, 0}, {4, 5}, {5, 6},
			{6, 7}, {7, 4}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};
		constexpr uint32 invalid_vertex = static_cast<uint32>(-1);
		const size_t cell_count = cell_side * cell_side * cell_side;
		DynamicArray<uint32> cell_vertices;
		cell_vertices.resize(cell_count, invalid_vertex);
		result.point_positions.reserve(cell_count / 2);

		for(int z = -1; z < static_cast<int>(cells_per_axis); ++z)
		{
			for(int y = -1; y < static_cast<int>(cells_per_axis); ++y)
			{
				for(int x = -1; x < static_cast<int>(cells_per_axis); ++x)
				{
					glm::dvec3 intersection_sum(0.0);
					uint8 intersection_count = 0;
					for(const auto& edge : cube_edges)
					{
						const int first_x = x + corner_offsets[edge[0]][0];
						const int first_y = y + corner_offsets[edge[0]][1];
						const int first_z = z + corner_offsets[edge[0]][2];
						const int second_x = x + corner_offsets[edge[1]][0];
						const int second_y = y + corner_offsets[edge[1]][1];
						const int second_z = z + corner_offsets[edge[1]][2];
						const double first_density = densities[sampleIndex(first_x, first_y, first_z)];
						const double second_density = densities[sampleIndex(second_x, second_y, second_z)];
						if((first_density >= 0.0) == (second_density >= 0.0)) continue;

						const double delta = first_density - second_density;
						const double interpolation = std::clamp(
							std::abs(delta) > 1e-12 ? first_density / delta : 0.5, 0.0, 1.0);
						intersection_sum += glm::mix(
							samplePosition(first_x, first_y, first_z),
							samplePosition(second_x, second_y, second_z), interpolation);
						++intersection_count;
					}
					if(intersection_count == 0) continue;

					const uint32 vertex_index = static_cast<uint32>(result.point_positions.size());
					result.point_positions.push(vec3(
						intersection_sum / static_cast<double>(intersection_count) - request.chunk_position));
					cell_vertices[cellIndex(x, y, z)] = vertex_index;
				}
			}
		}

		const auto appendQuad = [&result](uint32 a, uint32 b, uint32 c, uint32 d, const vec3& outward)
		{
			const vec3 pa = result.point_positions[a];
			const vec3 pb = result.point_positions[b];
			const vec3 pc = result.point_positions[c];
			const vec3 pd = result.point_positions[d];
			if(glm::dot(glm::cross(pb - pa, pc - pa), outward) < 0.0f)
			{
				const uint32 temporary = b;
				b = d;
				d = temporary;
			}
			const float diagonal_ac = glm::dot(pa - pc, pa - pc);
			const float diagonal_bd = glm::dot(pb - pd, pb - pd);
			if(diagonal_ac <= diagonal_bd)
			{
				result.triangle_point_indices.push(a);
				result.triangle_point_indices.push(b);
				result.triangle_point_indices.push(c);
				result.triangle_point_indices.push(a);
				result.triangle_point_indices.push(c);
				result.triangle_point_indices.push(d);
			}
			else
			{
				result.triangle_point_indices.push(a);
				result.triangle_point_indices.push(b);
				result.triangle_point_indices.push(d);
				result.triangle_point_indices.push(b);
				result.triangle_point_indices.push(c);
				result.triangle_point_indices.push(d);
			}
		};
		const auto emitEdgeQuad = [&](int x, int y, int z, uint8 axis)
		{
			const int next_x = x + (axis == 0);
			const int next_y = y + (axis == 1);
			const int next_z = z + (axis == 2);
			const double first_density = densities[sampleIndex(x, y, z)];
			const double second_density = densities[sampleIndex(next_x, next_y, next_z)];
			if((first_density >= 0.0) == (second_density >= 0.0)) return;

			uint32 vertices[4];
			if(axis == 0)
			{
				vertices[0] = cell_vertices[cellIndex(x, y - 1, z - 1)];
				vertices[1] = cell_vertices[cellIndex(x, y, z - 1)];
				vertices[2] = cell_vertices[cellIndex(x, y, z)];
				vertices[3] = cell_vertices[cellIndex(x, y - 1, z)];
			}
			else if(axis == 1)
			{
				vertices[0] = cell_vertices[cellIndex(x - 1, y, z - 1)];
				vertices[1] = cell_vertices[cellIndex(x - 1, y, z)];
				vertices[2] = cell_vertices[cellIndex(x, y, z)];
				vertices[3] = cell_vertices[cellIndex(x, y, z - 1)];
			}
			else
			{
				vertices[0] = cell_vertices[cellIndex(x - 1, y - 1, z)];
				vertices[1] = cell_vertices[cellIndex(x, y - 1, z)];
				vertices[2] = cell_vertices[cellIndex(x, y, z)];
				vertices[3] = cell_vertices[cellIndex(x - 1, y, z)];
			}
			for(uint32 vertex : vertices)
				if(vertex == invalid_vertex) return;
			const float direction = first_density >= 0.0 ? 1.0f : -1.0f;
			const vec3 outward = axis == 0 ? vec3(direction, 0.0f, 0.0f)
				: axis == 1 ? vec3(0.0f, direction, 0.0f) : vec3(0.0f, 0.0f, direction);
			appendQuad(vertices[0], vertices[1], vertices[2], vertices[3], outward);
		};

		for(int z = 0; z < static_cast<int>(cells_per_axis); ++z)
		{
			for(int y = 0; y < static_cast<int>(cells_per_axis); ++y)
			{
				for(int x = 0; x < static_cast<int>(cells_per_axis); ++x)
				{
					emitEdgeQuad(x, y, z, 0);
					emitEdgeQuad(x, y, z, 1);
					emitEdgeQuad(x, y, z, 2);
				}
			}
		}
		return result;
	}
}
