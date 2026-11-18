#pragma once

#include <danikk_space_engine/fwd.h>

namespace danikk_space_engine
{
	/**
	 * @brief Runs frame callbacks for active components attached to WorldBody instances.
	 * @param world World containing component slots and callback context.
	 */
	void renderWorldBodyComponents(World& world);

	/**
	 * @brief Runs frame callbacks for active point components in loaded chunks.
	 *
	 * This dispatches component callbacks only; chunk mesh and planet LOD drawing remain separate
	 * rendering stages owned by World and their respective rendering systems.
	 * @param world World containing component pools and loaded chunks.
	 */
	void renderChunkComponents(World& world);
}
