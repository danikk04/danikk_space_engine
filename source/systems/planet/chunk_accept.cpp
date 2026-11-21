#include <danikk_space_engine/profiler.h>
#include <danikk_space_engine/config.h>
#include <danikk_space_engine/systems/planet/system.h>
#include <danikk_space_engine/world.h>

namespace danikk_space_engine
{
	void PlanetSystem::acceptGeneratedSurfaceNode(const GeneratedChunkMesh& generated_mesh)
	{
		DSE_WATCH();
		if(generated_mesh.source != ChunkMeshSource::planet_surface_node) return;
		PlanetData* planet = findPlanet(generated_mesh.world_body_id);
		if(planet == nullptr || planet->generation_id != generated_mesh.planet_generation_id
			|| planet->terrain_revision != generated_mesh.terrain_revision
			|| generated_mesh.surface_node_id >= planet->surface_tree.nodes.size()
			|| generated_mesh.planet_mesh_generator != settings.planet_mesh_generator) return;
		PlanetSurfaceNode& node = planet->surface_tree.nodes[generated_mesh.surface_node_id];
		if(node.generation_id != generated_mesh.surface_node_generation_id) return;
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
	}
}
