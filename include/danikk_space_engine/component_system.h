#pragma once

#include <danikk_space_engine/fwd.h>
#include <danikk_space_engine/id.h>

namespace danikk_space_engine
{
	/**
	 * @brief Dispatches tick and frame callbacks for components in a World.
	 *
	 * The system owns no simulation data. It keeps update and render entry points separate and
	 * snapshots mutable membership lists before callbacks that can change the owning data pools.
	 */
	class ComponentSystem
	{
		/** @brief Owning World whose component callbacks are dispatched. */
		World& world;

	public:
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
	};
}
