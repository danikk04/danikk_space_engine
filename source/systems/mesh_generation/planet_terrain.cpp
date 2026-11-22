#include <danikk_space_engine/systems/telemetry/telemetry.h>
#include <danikk_space_engine/systems/mesh_generation/mesh_generation.h>
#include <danikk_space_engine/systems/planet/system.h>
#include <danikk_space_engine/world.h>
#include "internal.h"
#include <danikk_engine/danikk_engine.h>

namespace danikk_space_engine
{
	namespace
	{
		/**
		 * @brief Dispatches scalar-field meshing using the generator selected by the job.
		 * @param job Node-mesh settings and output arrays populated by the generator.
		 */
		void generatePlanetNodeGeometry(World& world, MeshGenerationJob& job)
		{
			switch(job.planet_mesh_generator)
			{
				case PlanetMeshGenerator::marching_tetrahedra:
					mesh_generation_internal::generatePlanetTerrainNodeMarchingTetrahedra(world, job);
					return;
				case PlanetMeshGenerator::surface_nets:
					mesh_generation_internal::generatePlanetTerrainNodeSurfaceNets(world, job);
					return;
			}
			danikk_engine::formatFatalError("Unsupported planet mesh generator %",
				static_cast<uint32>(job.planet_mesh_generator));
		}
	}

	/**
	 * @brief Generates a planet terrain node and assigns each surface triangle its material.
	 * @param job Generation identity, settings, edits, node bounds, and output arrays.
	 * The arrays may remain empty when the sampled field contains no surface in this node.
	 * This task-side function does not allocate World resources or upload a mesh.
	 */
	void mesh_generation_internal::generatePlanetTerrainNodeMesh(
		World& world, MeshGenerationJob& job)
	{
		/**
		 * @brief Measures the duration of the enclosing profiler scope.
		 */
		DSE_WATCH();
		const PlanetData* planet = world.planets.findPlanet(job.planet_id);
		if(planet == nullptr || planet->generation_id != job.planet_generation_id
			|| planet->terrain_revision != job.terrain_revision) return;
		generatePlanetNodeGeometry(world, job);

		const size_t triangle_count = job.triangle_point_indices.size() / 3;
		job.triangle_material_ids.reserve(triangle_count);
		for(size_t triangle = 0; triangle < triangle_count; triangle++)
		{
			const size_t first_index = triangle * 3;
			const vec3 a = job.point_positions[job.triangle_point_indices[first_index]];
			const vec3 b = job.point_positions[job.triangle_point_indices[first_index + 1]];
			const vec3 c = job.point_positions[job.triangle_point_indices[first_index + 2]];
			const glm::dvec3 center = job.node_minimum
				+ (glm::dvec3(a) + glm::dvec3(b) + glm::dvec3(c)) / 3.0;
			job.triangle_material_ids.push(world.planets.getPlanetMaterial(job.planet_id,
				center, job.surface_only
					? PlanetTerrainMode::heightmap_with_edits
					: PlanetTerrainMode::cave_field_with_edits));
		}
	}

}
