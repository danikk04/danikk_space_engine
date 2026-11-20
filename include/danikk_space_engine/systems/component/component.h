#pragma once

#include <danikk_space_engine/fwd.h>
#include <danikk_space_engine/id.h>
#include <danikk_framework/dynamic_array.h>

namespace danikk_space_engine
{
	/**
	 * @brief Dispatches tick and frame callbacks for components in a World.
	 *
	 * In addition to dispatching callbacks, the system queues point destruction until active
	 * component traversals finish. It snapshots mutable membership lists before callbacks that can
	 * change the owning data pools.
	 */
	class ComponentSystem
	{
		/** @brief Owning World whose component callbacks are dispatched. */
		World& world;
		/** @brief Point IDs queued until active component traversals finish. */
		DynamicArray<PointId> deferred_point_destruction_ids;

	public:
		/** @brief Clears deferred removals and logs subsystem initialization. */
		void init();
		/** @brief Clears deferred removals and logs subsystem shutdown. */
		void shutdown();
		/** @brief Binds this component system to its owning World. */
		explicit ComponentSystem(World& world) : world(world) {}

		/**
		 * @brief Ticks each active WorldBody's components and then that body's loaded chunks.
		 */
		void tickWorldBodyComponents();

		/**
		 * @brief Ticks active point components attached to one loaded chunk.
		 * @param chunk_id Loaded chunk whose component slots are traversed.
		 */
		void tickChunkComponents(ChunkId chunk_id);

		/** @brief Runs frame callbacks for active components attached to WorldBody instances. */
		void renderWorldBodyComponents();
		/** @brief Runs frame callbacks for active point components in loaded chunks. */
		void renderChunkComponents();
		/** @brief Queues a point for destruction after component traversal completes. */
		void deferPointDestruction(PointId point_id);
		/** @brief Destroys queued points through ChunkSystem after callback traversal. */
		void flushDeferredPointDestruction();
		/** @brief Drops queued point IDs during World reset or shutdown. */
		void clearDeferredPointDestruction();
	};
}
