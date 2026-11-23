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
		/** @brief Reusable stable slot snapshot for point-component lifecycle traversal. */
		DynamicArray<PointComponentSlotId> point_component_traversal_ids;

	public:
		/** @brief Clears deferred removals and logs subsystem initialization. */
		void init();
		/** @brief Clears deferred removals and logs subsystem shutdown. */
		void shutdown();
		/** @brief Binds this component system to its owning World. */
		explicit ComponentSystem(World& world) : world(world) {}

		/**
		 * @brief Ticks each active WorldBody's components.
		 */
		void tickWorldBodyComponents();

		/** @brief Ticks all active point components from a stable PointData slot snapshot. */
		void tickPointComponents();

		/** @brief Runs frame callbacks for active components attached to WorldBody instances. */
		void renderWorldBodyComponents();
		/** @brief Runs frame callbacks for active point components from a stable slot snapshot. */
		void renderPointComponents();
		/** @brief Attaches a point component and its point to the owning body's frame. */
		void attachPointComponentToBody(WorldBodyId body_id, PointId point_id,
			PointComponentSlotId slot_id, const glm::dvec3& body_local_position);
		/** @brief Queues a point for destruction after component traversal completes. */
		void deferPointDestruction(PointId point_id);
		/** @brief Destroys queued points after callback traversal. */
		void flushDeferredPointDestruction();
		/** @brief Drops queued point IDs during World reset or shutdown. */
		void clearDeferredPointDestruction();
	};
}
