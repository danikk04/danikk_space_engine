#pragma once

#include <danikk_space_engine/id.h>
#include <danikk_space_engine/fwd.h>
#include <danikk_framework/glm.h>

namespace danikk_space_engine
{
	/** @brief Strategic state shared by the registered units of one faction. */
	enum class UnitGlobalState : uint8
	{
		war,
		work,
		rest
	};

	/** @brief Current task within a unit's strategic state. */
	enum class UnitSubstate : uint8
	{
		pursuit,
		attack,
		reconnaissance,
		moving_to_work,
		collecting_corpses,
		sleep,
		idle
	};

	/** @brief Updates faction unit systems, perception, and NPC orders for one world tick. */
	void unitSystemTick(World& world, float dt);
	/** @brief Reports hostile fire so the threatened faction can reprioritize the attacker. */
	void reportUnitAttack(World& world, PointId attacker_point_id, PointId victim_point_id);
	/** @brief Registers a human with its faction's central AI system. */
	void registerUnitAgent(World& world, PointComponentSlotId human_slot_id, bool enabled = true);
	/** @brief Returns the current move target for a live registered unit, if it has one. */
	bool getUnitMoveTarget(World& world, PointComponentSlotId human_slot_id,
		uint64 generation_id, glm::dvec3& target_world);
	/** @brief Returns the current global state and substate of a registered live unit. */
	bool getUnitState(PointComponentSlotId human_slot_id, uint64 generation_id,
		UnitGlobalState& global_state, UnitSubstate& substate);
}
