#pragma once

#include <danikk_space_engine/fwd.h>

namespace danikk_space_engine
{
	/** @brief Updates faction unit systems, perception, and NPC orders for one world tick. */
	void unitSystemTick(World& world, float dt);
}
