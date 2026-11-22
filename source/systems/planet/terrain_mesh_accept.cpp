#include <danikk_space_engine/systems/telemetry/telemetry.h>
#include <danikk_space_engine/config.h>
#include <danikk_space_engine/systems/planet/system.h>
#include <danikk_space_engine/world.h>

namespace danikk_space_engine
{
	void PlanetSystem::acceptGeneratedSurfaceNode(const MeshGenerationJob& job)
	{
		DSE_WATCH();
		if(job.source != MeshGenerationSource::planet_terrain_node) return;
		const auto record = [](PlanetMeshAcceptance outcome)
			{ telemetry_system.recordPlanetMeshAcceptance(outcome); };
		PlanetData* planet = findPlanet(job.planet_id);
		if(planet == nullptr)
		{
			record(PlanetMeshAcceptance::missing_planet);
			return;
		}
		if(planet->generation_id != job.planet_generation_id)
		{
			record(PlanetMeshAcceptance::stale_planet);
			return;
		}
		if(planet->terrain_revision != job.terrain_revision)
		{
			record(PlanetMeshAcceptance::stale_terrain);
			return;
		}
		if(job.terrain_node_id >= planet->terrain_tree.nodes.size())
		{
			record(PlanetMeshAcceptance::invalid_node);
			return;
		}
		if(job.planet_mesh_generator != settings.planet_mesh_generator)
		{
			record(PlanetMeshAcceptance::stale_generator);
			return;
		}
		PlanetTerrainNode& node = planet->terrain_tree.nodes[job.terrain_node_id];
		if(node.generation_id != job.terrain_node_generation_id)
		{
			record(PlanetMeshAcceptance::stale_node);
			return;
		}
		node.generation_pending = false;
		node.mesh_position = job.node_minimum;
		createMeshRenderData(node.mesh,
			job.point_positions.data(), job.point_positions.size(),
			job.triangle_point_indices.data(), job.triangle_point_indices.size(),
			job.triangle_material_ids.data(), job.triangle_material_ids.size());
		node.mesh_revision = job.terrain_revision;
		node.mesh_generator = job.planet_mesh_generator;
		node.mesh_surface_only = job.surface_only;
		node.render_active = false;
		record(job.triangle_point_indices.empty()
			? PlanetMeshAcceptance::empty : PlanetMeshAcceptance::accepted);
	}
}
