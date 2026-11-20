#include <danikk_space_engine/tool.h>
#include <danikk_space_engine/world.h>
#include <danikk_space_engine/systems/planet/system.h>

namespace danikk_space_engine
{
	namespace
	{

		/**
		 * @brief Checks whether an active chunk is owned by a body with a live Planet component.
		 * @param world World used to resolve the chunk, body, and component slots.
		 * @param chunk_id Candidate chunk ID.
		 * @return True when the full ownership chain is active and contains a planet component.
		 */
		bool isPlanetChunk(const World& world, ChunkId chunk_id)
		{
			if(!world.chunks.isChunkActive(chunk_id)) return false;
			const Chunk& chunk = world.chunks.getChunk(chunk_id);
			if(!world.world_body_data.isWorldBodyActive(chunk.world_body_id)) return false;
			return world.planets.isPlanet(chunk.world_body_id);
		}
	}

	/**
	 * @brief Applies a six-metre-radius additive or subtractive stamp to a planet hit.
	 * @param world World containing the target planet.
	 * @param action Terrain action; only extrude and erase are recognized.
	 * @param hit Raycast result defining the triangle-centered edit.
	 * @return Empty selection result; misses, non-planet chunks, and unsupported actions do nothing.
	 */
	ToolApplyResult applyTerrainTool(World& world, ToolActionId action, const RaycastHit& hit)
	{
		if(!hit.hit || hit.hit_build_part || !isPlanetChunk(world, hit.chunk_id)) return ToolApplyResult();
		switch(action)
		{
			case ToolActionId::extrude:
				world.planets.editGround(hit.body_id, hit.position, 6.0, true);
				break;
			case ToolActionId::erase:
				world.planets.editGround(hit.body_id, hit.position, 6.0, false);
				break;
			default:
				break;
		}
		return ToolApplyResult();
	}
}
