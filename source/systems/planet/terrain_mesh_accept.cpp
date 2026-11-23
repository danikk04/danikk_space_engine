#include <danikk_space_engine/systems/telemetry/telemetry.h>
#include <danikk_space_engine/config.h>
#include <danikk_space_engine/systems/planet/system.h>
#include <danikk_space_engine/world.h>

#include <utility>

namespace danikk_space_engine
{
	void PlanetSystem::acceptGeneratedTerrainNodeMesh(const MeshGenerationJob& job)
	{
		DSE_WATCH();
		if(job.source != MeshGenerationSource::planet_terrain_node) return;
		const auto record = [this](PlanetMeshAcceptance outcome)
			{ world.getTelemetrySystem().mesh_generation_telemetry.recordPlanetMeshAcceptance(outcome); };
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
		if(job.terrain_node_id >= planet->terrain_tree.nodes.size())
		{
			record(PlanetMeshAcceptance::invalid_node);
			return;
		}
		PlanetTerrainNode& node = planet->terrain_tree.nodes[job.terrain_node_id];
		if(node.generation_id != job.terrain_node_generation_id)
		{
			record(PlanetMeshAcceptance::stale_node);
			return;
		}
		if(planet->terrain_revision != job.terrain_revision)
		{
			setMeshGenerationJobPending(job, false);
			record(PlanetMeshAcceptance::stale_terrain);
			return;
		}
		if(job.planet_mesh_generator != settings.planet_mesh_generator)
		{
			setMeshGenerationJobPending(job, false);
			record(PlanetMeshAcceptance::stale_generator);
			return;
		}
		node.mesh_position = job.node_minimum;
		// Build into a separate GPU mesh so the currently visible buffers remain
		// intact until the replacement has been uploaded completely.
		MeshRenderData replacement;
		createMeshRenderData(replacement,
			job.point_positions.data(), job.point_positions.size(),
			job.triangle_point_indices.data(), job.triangle_point_indices.size(),
			job.triangle_material_ids.data(), job.triangle_material_ids.size());

		MeshRenderData previous;
		previous.mesh = node.mesh.mesh;
		previous.material_ranges = std::move(node.mesh.material_ranges);
		node.mesh.mesh = replacement.mesh;
		replacement.mesh = nullptr;
		node.mesh.material_ranges = std::move(replacement.material_ranges);
		destroyMeshRenderData(previous);

		node.mesh_revision = job.terrain_revision;
		node.mesh_generated = true;
		node.mesh_generator = job.planet_mesh_generator;
		node.mesh_surface_only = job.surface_only;
		setMeshGenerationJobPending(job, false);
		record(job.triangle_point_indices.empty()
			? PlanetMeshAcceptance::empty : PlanetMeshAcceptance::accepted);
	}
}
