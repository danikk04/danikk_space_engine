#include <danikk_space_engine/profiler.h>
#include <danikk_space_engine/config.h>
#include <danikk_space_engine/systems/planet/system.h>
#include <danikk_space_engine/world.h>
#include <danikk_engine/danikk_engine.h>
#include <danikk_framework/log.h>

namespace danikk_space_engine
{
	void PlanetSystem::acceptGeneratedSurfaceNode(const GeneratedChunkMesh& generated_mesh)
	{
		DSE_WATCH();
		if(generated_mesh.source != ChunkMeshSource::planet_surface_node) return;
		constexpr bool log_planet_mesh_acceptance_diagnostics = true;
		static uint32 report_timestamp_ms = 0;
		static size_t accepted = 0;
		static size_t empty = 0;
		static size_t missing_planet = 0;
		static size_t stale_planet = 0;
		static size_t stale_terrain = 0;
		static size_t invalid_node = 0;
		static size_t stale_generator = 0;
		static size_t stale_node = 0;
		const auto report_diagnostics = [&]()
		{
			if constexpr(log_planet_mesh_acceptance_diagnostics)
			{
				const uint32 now = danikk_engine::getMsTimestamp();
				if(report_timestamp_ms == 0) report_timestamp_ms = now;
				if(now - report_timestamp_ms >= 1000)
				{
					danikk_framework::formatLogInfo(
						"Planet mesh acceptance (last 1 s): accepted=% empty=% missing_planet=% stale_planet=% stale_terrain=% invalid_node=% stale_generator=% stale_node=%",
						accepted, empty, missing_planet, stale_planet, stale_terrain,
						invalid_node, stale_generator, stale_node);
					accepted = 0;
					empty = 0;
					missing_planet = 0;
					stale_planet = 0;
					stale_terrain = 0;
					invalid_node = 0;
					stale_generator = 0;
					stale_node = 0;
					report_timestamp_ms = now;
				}
			}
		};
		PlanetData* planet = findPlanet(generated_mesh.world_body_id);
		if(planet == nullptr)
		{
			missing_planet++;
			report_diagnostics();
			return;
		}
		if(planet->generation_id != generated_mesh.planet_generation_id)
		{
			stale_planet++;
			report_diagnostics();
			return;
		}
		if(planet->terrain_revision != generated_mesh.terrain_revision)
		{
			stale_terrain++;
			report_diagnostics();
			return;
		}
		if(generated_mesh.surface_node_id >= planet->surface_tree.nodes.size())
		{
			invalid_node++;
			report_diagnostics();
			return;
		}
		if(generated_mesh.planet_mesh_generator != settings.planet_mesh_generator)
		{
			stale_generator++;
			report_diagnostics();
			return;
		}
		PlanetSurfaceNode& node = planet->surface_tree.nodes[generated_mesh.surface_node_id];
		if(node.generation_id != generated_mesh.surface_node_generation_id)
		{
			stale_node++;
			report_diagnostics();
			return;
		}
		node.generation_pending = false;
		if(node.mesh_chunk_id == static_cast<ChunkId>(-1)
			|| !world.chunks.isChunkActive(node.mesh_chunk_id))
			node.mesh_chunk_id = world.chunks.createChunk(planet->world_body_id).chunk_id;
		Chunk& chunk = world.chunks.getChunk(node.mesh_chunk_id);
		chunk.position = generated_mesh.chunk_position;
		world.chunks.createMesh(chunk,
			generated_mesh.point_positions.data(), generated_mesh.point_positions.size(),
			generated_mesh.triangle_point_indices.data(), generated_mesh.triangle_point_indices.size(),
			generated_mesh.triangle_material_ids.data(), generated_mesh.triangle_material_ids.size());
		chunk.render_enabled = false;
		node.mesh_revision = generated_mesh.terrain_revision;
		node.mesh_generator = generated_mesh.planet_mesh_generator;
		node.mesh_surface_only = generated_mesh.surface_only;
		node.render_active = false;
		accepted++;
		if(generated_mesh.triangle_point_indices.empty()) empty++;
		report_diagnostics();
	}
}
