#include <danikk_space_engine/profiler.h>
#include <danikk_space_engine/systems/chunk_mesh_generation/chunk_mesh_generation.h>
#include <danikk_space_engine/systems/planet/system.h>
#include <danikk_space_engine/world.h>
#include "internal.h"
#include "mesh_algorithms.h"
#include <danikk_engine/danikk_engine.h>

namespace danikk_space_engine
{
	namespace
	{
		/**
		 * @brief Dispatches scalar-field meshing to the generator selected by the request.
		 * @param request Immutable chunk-generation settings and generator selection.
		 * @return Geometry produced by the selected mesh algorithm.
		 */
		GeneratedChunkMesh generatePlanetChunkGeometry(World& world, const ChunkMeshGenerationRequest& request)
		{
			switch(request.planet_mesh_generator)
			{
				case PlanetMeshGenerator::marching_tetrahedra:
					return chunk_mesh_generation_internal::generatePlanetChunkMarchingTetrahedra(world, request);
				case PlanetMeshGenerator::surface_nets:
					return chunk_mesh_generation_internal::generatePlanetTerrainChunkMeshSurfaceNets(world, request);
			}
			danikk_engine::formatFatalError("Unsupported planet mesh generator %",
				static_cast<uint32>(request.planet_mesh_generator));
			return {};
		}
	}

	/**
	 * @brief Generates a planet chunk and assigns each surface triangle its material.
	 * @param request Immutable generation identity, settings, edits, and chunk coordinate.
	 * @return Worker result containing request identity, local geometry, and a
	 *         material ID for each triangle; geometry arrays may be empty when the
	 *         sampled field contains no surface in this chunk.
	 * This worker-side function does not allocate World resources or upload a mesh.
	 */
	GeneratedChunkMesh chunk_mesh_generation_internal::generatePlanetTerrainChunkMesh(
		World& world, const ChunkMeshGenerationRequest& request)
	{
		/**
		 * @brief Measures the duration of the enclosing profiler scope.
		 */
		DSE_WATCH();
		const PlanetData* planet = world.planets.findPlanet(request.world_body_id);
		if(planet == nullptr || planet->generation_id != request.planet_generation_id
			|| planet->terrain_revision != request.terrain_revision) return {};
		GeneratedChunkMesh result = generatePlanetChunkGeometry(world, request);

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
			result.triangle_material_ids.push(world.planets.getPlanetMaterial(request.world_body_id, center));
		}
		return result;
	}

	/**
	 * @brief Generates one cave-free cube-sphere face for an orbital planet LOD.
	 * @param request Face index, level resolution, and immutable planet surface data.
	 * @return Planet-local indexed geometry with material IDs sampled from the surface.
	 * The generator samples only the radial terrain surface and terrain stamps; it does
	 * not evaluate the cave field or create chunks and point resources.
	 */
	GeneratedChunkMesh chunk_mesh_generation_internal::generatePlanetSurfaceLodMesh(
		World& world, const ChunkMeshGenerationRequest& request)
	{
		DSE_WATCH();
		const PlanetData* planet = world.planets.findPlanet(request.world_body_id);
		if(planet == nullptr || planet->generation_id != request.planet_generation_id
			|| planet->terrain_revision != request.terrain_revision) return {};
		if(request.source != ChunkMeshSource::planet_surface_lod
			|| request.lod_level == 0 || request.lod_level >= planet_chunk_level_count
			|| request.lod_resolution == 0)
			danikk_engine::formatFatalError("Planet surface LOD request has invalid data");

		if(request.lod_level == 1)
		{
			if(request.lod_face >= planet_coarse_chunk_count)
				danikk_engine::formatFatalError("Coarse planet chunk request has invalid patch index");
			GeneratedChunkMesh result = generatePlanetChunkGeometry(world, request);
			result.lod_resolution = request.lod_resolution;
			result.lod_face = request.lod_face;
			result.lod_level = request.lod_level;
			result.chunk_position = request.chunk_position;
			const size_t triangle_count = result.triangle_point_indices.size() / 3;
			result.triangle_material_ids.reserve(triangle_count);
			for(size_t triangle = 0; triangle < triangle_count; triangle++)
			{
				const size_t first_index = triangle * 3;
				const uint32 a = result.triangle_point_indices[first_index];
				const uint32 b = result.triangle_point_indices[first_index + 1];
				const uint32 c = result.triangle_point_indices[first_index + 2];
				const glm::dvec3 center = request.chunk_position
					+ (glm::dvec3(result.point_positions[a]) + glm::dvec3(result.point_positions[b])
						+ glm::dvec3(result.point_positions[c])) / 3.0;
				result.triangle_material_ids.push(
					world.planets.getPlanetMaterial(request.world_body_id, center));
			}
			return result;
		}

		const bool local_heightmap = request.lod_level == 2 || request.lod_level == 3;
		if(local_heightmap && (!request.has_heightmap_basis
			|| request.lod_face >= planet_local_heightmap_tile_count))
			danikk_engine::formatFatalError("Planet local heightmap request has invalid patch data");
		if(!local_heightmap && request.lod_face >= planet_lod_face_count)
			danikk_engine::formatFatalError("Planet heightmap request has invalid patch index");

		GeneratedChunkMesh result;
		result.generation_id = request.generation_id;
		result.source_revision = request.source_revision;
		result.world_body_id = request.world_body_id;
		result.planet_generation_id = request.planet_generation_id;
		result.terrain_revision = request.terrain_revision;
		result.source = request.source;
		result.lod_resolution = request.lod_resolution;
		result.lod_face = request.lod_face;
		result.lod_level = request.lod_level;
		result.grid_coordinate = request.grid_coordinate;
		result.chunk_position = request.chunk_position;

		const size_t resolution = request.lod_resolution;
		const double minimum_terrain_wavelength = request.voxel_size * 3.0;
		const size_t points_per_axis = resolution + 1;
		const auto point_id = [points_per_axis](size_t x, size_t y)
		{
			return static_cast<uint32>(y * points_per_axis + x);
		};
		result.point_positions.reserve(points_per_axis * points_per_axis);
		if(local_heightmap)
		{
			for(size_t y = 0; y <= resolution; y++)
			{
				for(size_t x = 0; x <= resolution; x++)
				{
					const double u = (static_cast<double>(x) / resolution - 0.5) * request.chunk_width;
					const double v = (static_cast<double>(y) / resolution - 0.5) * request.chunk_width;
					const glm::dvec3 tangent_position = request.chunk_position
						+ request.heightmap_right * u + request.heightmap_forward * v;
					const glm::dvec3 direction = glm::normalize(tangent_position);
					const double radius = world.planets.getPlanetSurfaceRadius(
						request.world_body_id, direction, minimum_terrain_wavelength);
					result.point_positions.push(vec3(direction * radius - request.chunk_position));
				}
			}
		}
		else
		{
			for(size_t y = 0; y <= resolution; y++)
			{
				for(size_t x = 0; x <= resolution; x++)
				{
					const double u = -1.0 + 2.0 * static_cast<double>(x) / resolution;
					const double v = -1.0 + 2.0 * static_cast<double>(y) / resolution;
					glm::dvec3 cube_direction;
					switch(request.lod_face)
					{
						case 0: cube_direction = glm::dvec3(1.0, u, v); break;
						case 1: cube_direction = glm::dvec3(-1.0, -u, v); break;
						case 2: cube_direction = glm::dvec3(u, 1.0, v); break;
						case 3: cube_direction = glm::dvec3(u, -1.0, -v); break;
						case 4: cube_direction = glm::dvec3(u, v, 1.0); break;
						default: cube_direction = glm::dvec3(-u, v, -1.0); break;
					}
					const glm::dvec3 direction = glm::normalize(cube_direction);
					const double radius = world.planets.getPlanetSurfaceRadius(
						request.world_body_id, direction, minimum_terrain_wavelength);
					result.point_positions.push(vec3(direction * radius));
				}
			}
		}

		const auto append_triangle = [&world, &result, &request, minimum_terrain_wavelength](uint32 a, uint32 b, uint32 c)
		{
			const vec3& pa = result.point_positions[a];
			const vec3& pb = result.point_positions[b];
			const vec3& pc = result.point_positions[c];
			const glm::dvec3 absolute_a = request.chunk_position + glm::dvec3(pa);
			const glm::dvec3 absolute_b = request.chunk_position + glm::dvec3(pb);
			const glm::dvec3 absolute_c = request.chunk_position + glm::dvec3(pc);
			if(glm::dot(glm::dvec3(glm::cross(pb - pa, pc - pa)), absolute_a + absolute_b + absolute_c) < 0.0)
			{
				const uint32 temporary = b;
				b = c;
				c = temporary;
			}
			result.triangle_point_indices.push(a);
			result.triangle_point_indices.push(b);
			result.triangle_point_indices.push(c);
			const glm::dvec3 center = request.chunk_position + (glm::dvec3(result.point_positions[a])
				+ glm::dvec3(result.point_positions[b]) + glm::dvec3(result.point_positions[c])) / 3.0;
			const glm::dvec3 direction = glm::normalize(center);
			const glm::dvec3 surface_position = direction
				* world.planets.getPlanetSurfaceRadius(
					request.world_body_id, direction, minimum_terrain_wavelength);
			result.triangle_material_ids.push(
				world.planets.getPlanetMaterial(request.world_body_id, surface_position));
		};
		result.triangle_point_indices.reserve(resolution * resolution * 6);
		result.triangle_material_ids.reserve(resolution * resolution * 2);
		for(size_t y = 0; y < resolution; y++)
		{
			for(size_t x = 0; x < resolution; x++)
			{
				const uint32 lower_left = point_id(x, y);
				const uint32 lower_right = point_id(x + 1, y);
				const uint32 upper_right = point_id(x + 1, y + 1);
				const uint32 upper_left = point_id(x, y + 1);
				append_triangle(lower_left, lower_right, upper_right);
				append_triangle(lower_left, upper_right, upper_left);
			}
		}
		return result;
	}
}
