#include <danikk_space_engine/tool.h>
#include <danikk_space_engine/world/world.h>
#include <danikk_space_engine/component/world_body/planet/planet.h>

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
			if(!world.chunk_system.isChunkActive(chunk_id)) return false;
			const Chunk& chunk = world.chunk_system.getChunk(chunk_id);
			if(!world.world_body_data.isWorldBodyActive(chunk.world_body_id)) return false;
			const WorldBody& body = world.world_body_data.getWorldBody(chunk.world_body_id);
			for(WorldBodyComponentSlotId slot_id : body.component_slot_ids)
			{
				if(world.world_body_data.isWorldBodyComponentSlotActive(slot_id)
					&& world.world_body_data.getWorldBodyComponentSlot(slot_id).type_id == WorldBodyComponentTypeId::planet)
				{
					return true;
				}
			}
			return false;
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
				editPlanetGround(world, hit.body_id, hit.position, 6.0, true);
				break;
			case ToolActionId::erase:
				editPlanetGround(world, hit.body_id, hit.position, 6.0, false);
				break;
			default:
				break;
		}
		return ToolApplyResult();
	}
}
