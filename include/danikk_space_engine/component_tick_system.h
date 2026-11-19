#pragma once

#include <danikk_space_engine/fwd.h>
#include <danikk_space_engine/id.h>

namespace danikk_space_engine
{
	/**
	 * @brief Dispatches tick and frame callbacks for components in a World.
	 *
	 * The system owns no simulation data. It keeps update and render entry points separate and
	 * snapshots mutable membership lists before callbacks that can change WorldData.
	 */
	class ComponentSystem
	{
	public:
		/**
		 * @brief Ticks each active WorldBody's components and then that body's loaded chunks.
		 * @param world World containing component slots and loaded chunks.
		 */
		void tickWorldBodyComponents(World& world);

		/**
		 * @brief Ticks active point components attached to one loaded chunk.
		 * @param world World providing component pools and callback context.
		 * @param chunk_id Loaded chunk whose component slots are traversed.
		 */
		void tickChunkComponents(World& world, ChunkId chunk_id);

		/** @brief Runs frame callbacks for active components attached to WorldBody instances. */
		void renderWorldBodyComponents(World& world);
		/** @brief Runs frame callbacks for active point components in loaded chunks. */
		void renderChunkComponents(World& world);
	};
}
