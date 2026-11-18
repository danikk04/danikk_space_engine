#include <danikk_framework/glm.h>

#include <danikk_space_engine/profiler.h>
#include <danikk_space_engine/world/world.h>
#include <danikk_space_engine/unit_system.h>
#include <danikk_space_engine/component/point/human/npc_controller.h>

namespace danikk_space_engine
{
	/**
	 * @brief Ticks body and chunk components, drains deferred point deletion, then advances physics.
	 * Body, chunk, and per-body component IDs are snapshotted before callbacks can mutate ownership lists.
	 * Deferred cleanup completes before UnitSystem updates faction intelligence and orders. WorldPhysics
	 * then advances motion and resolves contacts.
	 */
	void World::tick()
	{
		/**
		 * @brief Measures the duration of the enclosing profiler scope.
		 */
		DSE_WATCH();
		{
			/**
			 * @brief Measures the duration of the enclosing profiler scope.
			 */
			DSE_WATCH_SCOPE("WorldBody components");
			const DynamicArray<WorldBodyId> world_body_ids(data.getUsedWorldBodyIds());
			DynamicArray<WorldBodyComponentSlotId> component_slot_ids;
			DynamicArray<ChunkId> chunk_ids;
			for(WorldBodyId body_id : world_body_ids)
			{
				if(!data.isWorldBodyActive(body_id)) continue;
				component_slot_ids.clear();
				chunk_ids.clear();
				{
					const WorldBody& body = data.getWorldBody(body_id);
					for(WorldBodyComponentSlotId slot_id : body.component_slot_ids)
					{
						component_slot_ids.push(slot_id);
					}
					for(ChunkId chunk_id : body.chunk_ids)
					{
						chunk_ids.push(chunk_id);
					}
				}

				for(WorldBodyComponentSlotId slot_id : component_slot_ids)
				{
					if(!data.isWorldBodyComponentSlotActive(slot_id)) continue;
					WorldBodyComponentSlot& slot = data.getWorldBodyComponentSlot(slot_id);
					const WorldBodyComponentType& type = getWorldBodyComponentType(slot.type_id);
					if(type.tick != nullptr)
					{
						type.tick(slot, *this);
					}
				}
				for(ChunkId chunk_id : chunk_ids)
				{
					if(!data.isChunkActive(chunk_id)) continue;
					data.getChunk(chunk_id).tick(*this);
				}
			}
		}
		for(PointComponentSlotId slot_id : pending_npc_chunk_updates) updateNpcChunkAfterTick(*this, slot_id);
		pending_npc_chunk_updates.clear();
		for(PointId point_id : pending_point_destruction_ids)
		{
			if(data.isPointActive(point_id)) data.destroyPoint(point_id);
		}
		pending_point_destruction_ids.clear();
		unitSystemTick(*this, glm::max(getTargetFrameDelay(), 0.0f));
		physics.tick(*this);
	}
}
