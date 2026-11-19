#include <danikk_space_engine/profiler.h>
#include <danikk_space_engine/config.h>
#include <danikk_space_engine/component/world_body/planet/planet.h>
#include <danikk_space_engine/world/world.h>

namespace danikk_space_engine
{
	namespace
	{
		/** @brief Compares integer planet-grid coordinates component by component. */
		bool sameChunkKey(const glm::ivec3& left, const glm::ivec3& right)
		{
			return left.x == right.x && left.y == right.y && left.z == right.z;
		}

		/** @brief Removes an array element while retaining the order of the remaining entries. */
		template<typename value_t>
		void eraseAt(DynamicArray<value_t>& values, size_t index)
		{
			for(size_t shift = index + 1; shift < values.size(); shift++)
				values[shift - 1] = static_cast<value_t&&>(values[shift]);
			values.resize(values.size() - 1);
		}
	}

	/**
	 * @brief Validates a worker result and uploads its temporary mesh arrays into a chunk GPU resource.
	 * @param slot Planet component slot that requested this result.
	 * @param world World whose main-thread context accepts the generated data.
	 * @param generated_chunk CPU-generated positions, triangle indices, and per-triangle materials.
	 * Stale or no-longer-desired results are discarded. Valid non-empty chunks are
	 * uploaded synchronously, then the temporary arrays can be released. WorldData
	 * retains the chunk and its GPU mesh, but no copy of terrain triangles or vertices.
	 * Empty results are recorded in the lookup without allocating a chunk.
	 */
	void acceptPlanetGeneratedChunk(
		WorldBodyComponentSlot& slot,
		World& world,
		const GeneratedChunkMesh& generated_chunk)
	{
		DSE_WATCH();
		PlanetComponent& planet = *static_cast<PlanetComponent*>(slot.getData());
		if(generated_chunk.terrain_revision != planet.terrain_revision
			|| generated_chunk.source != ChunkMeshSource::planet_terrain
			|| generated_chunk.planet_mesh_generator != settings.planet_mesh_generator
			|| !planet.chunk_lookup.isDesired(generated_chunk.grid_coordinate)
			|| planet.chunk_lookup.isGenerated(
				generated_chunk.grid_coordinate, generated_chunk.planet_mesh_generator)) return;

		ChunkId chunk_id = static_cast<ChunkId>(-1);
		planet.chunk_lookup.getChunk(generated_chunk.grid_coordinate, chunk_id);
		const bool has_geometry = generated_chunk.triangle_point_indices.size() > 0;
		if(world.chunk_system.isChunkActive(chunk_id) || has_geometry)
		{
			if(!world.chunk_system.isChunkActive(chunk_id))
				chunk_id = world.chunk_system.createChunk(world, slot.world_body_id).chunk_id;
			Chunk& chunk = world.chunk_system.getChunk(chunk_id);
			chunk.grid_coordinate = generated_chunk.grid_coordinate;
			chunk.position = generated_chunk.chunk_position;
			world.chunk_system.createMesh(chunk,
				generated_chunk.point_positions.data(), generated_chunk.point_positions.size(),
				generated_chunk.triangle_point_indices.data(), generated_chunk.triangle_point_indices.size(),
				generated_chunk.triangle_material_ids.data(), generated_chunk.triangle_material_ids.size());
		}
		planet.chunk_lookup.setChunk(
			generated_chunk.grid_coordinate, chunk_id, generated_chunk.planet_mesh_generator);
		world.removeChunkMeshGenerationRequest(slot.slot_id, generated_chunk.grid_coordinate);
		for(size_t index = 0; index < planet.pending_requests.size(); index++)
		{
			if(!sameChunkKey(planet.pending_requests[index].grid_coordinate, generated_chunk.grid_coordinate)) continue;
			eraseAt(planet.pending_requests, index);
			break;
		}
	}
}
