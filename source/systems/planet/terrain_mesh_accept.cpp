#include <danikk_space_engine/systems/telemetry/telemetry.h>
#include <danikk_space_engine/config.h>
#include <danikk_space_engine/systems/planet/system.h>
#include <danikk_space_engine/world.h>

namespace danikk_space_engine
{
	void PlanetSystem::acceptGeneratedSurfaceNode(const GeneratedMesh& generated_mesh)
	{
		DSE_WATCH();
		if(generated_mesh.source != MeshGenerationSource::planet_terrain_node) return;
		const auto record = [](PlanetMeshAcceptance outcome)
			{ telemetry_system.recordPlanetMeshAcceptance(outcome); };
		PlanetData* planet = findPlanet(generated_mesh.world_body_id);
		if(planet == nullptr)
		{
			record(PlanetMeshAcceptance::missing_planet);
			return;
		}
		if(planet->generation_id != generated_mesh.planet_generation_id)
		{
			record(PlanetMeshAcceptance::stale_planet);
			return;
		}
		if(planet->terrain_revision != generated_mesh.terrain_revision)
		{
			record(PlanetMeshAcceptance::stale_terrain);
			return;
		}
		if(generated_mesh.terrain_node_id >= planet->terrain_tree.nodes.size())
		{
			record(PlanetMeshAcceptance::invalid_node);
			return;
		}
		if(generated_mesh.planet_mesh_generator != settings.planet_mesh_generator)
		{
			record(PlanetMeshAcceptance::stale_generator);
			return;
		}
		PlanetTerrainNode& node = planet->terrain_tree.nodes[generated_mesh.terrain_node_id];
		if(node.generation_id != generated_mesh.terrain_node_generation_id)
		{
			record(PlanetMeshAcceptance::stale_node);
			return;
		}
		node.generation_pending = false;
		node.mesh_position = generated_mesh.node_minimum;
		createMeshRenderData(node.mesh,
			generated_mesh.point_positions.data(), generated_mesh.point_positions.size(),
			generated_mesh.triangle_point_indices.data(), generated_mesh.triangle_point_indices.size(),
			generated_mesh.triangle_material_ids.data(), generated_mesh.triangle_material_ids.size());
		node.mesh_revision = generated_mesh.terrain_revision;
		node.mesh_generator = generated_mesh.planet_mesh_generator;
		node.mesh_surface_only = generated_mesh.surface_only;
		node.render_active = false;
		record(generated_mesh.triangle_point_indices.empty()
			? PlanetMeshAcceptance::empty : PlanetMeshAcceptance::accepted);
	}
}
