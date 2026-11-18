#pragma once

#include <danikk_space_engine/id.h>
#include <danikk_space_engine/fwd.h>

namespace danikk_space_engine
{
	/** @brief Updates faction unit systems, perception, and NPC orders for one world tick. */
	void unitSystemTick(World& world, float dt);
	/** @brief Reports hostile fire so the threatened faction can reprioritize the attacker. */
	void reportUnitAttack(World& world, PointId attacker_point_id, PointId victim_point_id);
}
