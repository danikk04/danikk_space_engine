#include <danikk_framework/glm.h>

#include <danikk_space_engine/profiler.h>
#include <danikk_space_engine/component_tick_system.h>
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
			tickWorldBodyComponents(*this);
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
