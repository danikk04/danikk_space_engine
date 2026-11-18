#pragma once

#include <danikk_space_engine/fwd.h>
#include <danikk_space_engine/id.h>

namespace danikk_space_engine
{
	/**
	 * @brief Ticks each active WorldBody's components and then that body's loaded chunks.
	 *
	 * Body and chunk membership lists are snapshotted before callbacks for each body, preserving
	 * the existing update order while allowing callbacks to create or remove resources safely.
	 * @param world World containing component slots and loaded chunks.
	 */
	void tickWorldBodyComponents(World& world);

	/**
	 * @brief Ticks active point components attached to one loaded chunk.
	 * @param world World providing component pools and callback context.
	 * @param chunk_id Loaded chunk whose component slots are traversed.
	 */
	void tickChunkComponents(World& world, ChunkId chunk_id);
}
