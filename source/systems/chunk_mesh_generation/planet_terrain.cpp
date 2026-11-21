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
	GeneratedChunkMesh chunk_mesh_generation_internal::generatePlanetSurfaceNodeMesh(
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
			result.triangle_material_ids.push(world.planets.getPlanetMaterial(request.world_body_id,
				center, request.surface_only
					? PlanetTerrainMode::heightmap_with_edits
					: PlanetTerrainMode::cave_field_with_edits));
		}
		return result;
	}

}
