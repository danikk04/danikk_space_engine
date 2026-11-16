#include <danikk_space_engine/tool.h>
#include <danikk_space_engine/world/world.h>
#include <danikk_space_engine/component/world_body/planet/planet.h>

namespace danikk_space_engine
{
	namespace
	{

		/**
		 * @brief Checks whether the chunk belongs to a world body with an active planet component.
		 */
		bool isPlanetChunk(const World& world, ChunkId chunk_id)
		{
			if(!world.data.isChunkActive(chunk_id)) return false;
			const Chunk& chunk = world.data.getChunk(chunk_id);
			if(!world.data.isWorldBodyActive(chunk.world_body_id)) return false;
			const WorldBody& body = world.data.getWorldBody(chunk.world_body_id);
			for(WorldBodyComponentSlotId slot_id : body.component_slot_ids)
			{
				if(world.data.isWorldBodyComponentSlotActive(slot_id)
					&& world.data.getWorldBodyComponentSlot(slot_id).type_id == WorldBodyComponentTypeId::planet)
				{
					return true;
				}
			}
			return false;
		}
	}

	/**
	 * @brief Applies terrain tool.
	 */
	ToolApplyResult applyTerrainTool(World& world, ToolActionId action, const RaycastHit& hit)
	{
		if(!hit.hit || !isPlanetChunk(world, hit.face.chunk_id)) return ToolApplyResult();
		switch(action)
		{
			case ToolActionId::extrude:
				editPlanetGround(world, hit.face, 6.0, true);
				break;
			case ToolActionId::erase:
				editPlanetGround(world, hit.face, 6.0, false);
				break;
			default:
				break;
		}
		return ToolApplyResult();
	}
}
