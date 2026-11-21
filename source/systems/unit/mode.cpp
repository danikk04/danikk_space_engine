#include <danikk_space_engine/systems/telemetry/telemetry.h>
#include <danikk_space_engine/systems/unit/internal/mode.h>
#include <danikk_space_engine/systems/unit/internal/state.h>

namespace danikk_space_engine::unit_system_internal
{
	/** @brief Applies group-wide combat entry and exit rules after perception updates. */
	void updateGroupMode(UnitGroup& group, float dt)
	{
		DSE_WATCH();
		const bool was_in_battle = group.war_active;
		if(group.enemy_seen_this_tick || group.attack_reported_this_tick)
		{
			group.has_had_enemy_contact = true;
			group.war_active = true;
			if(!was_in_battle) group.time_since_enemy_visible = 0.0f;
		}

		if(group.enemy_seen_this_tick)
		{
			group.time_since_enemy_visible = 0.0f;
		}
		else if(group.war_active)
		{
			group.time_since_enemy_visible += glm::max(dt, 0.0f);
			if(group.time_since_enemy_visible >= 10.0f)
			{
				group.war_active = false;
				group.known_sightings.clear();
				for(UnitSearchLocation& location : group.search_locations)
					if(!location.is_corpse)
					{
						location.is_corpse = true;
						location.assigned_unit_slot_id = static_cast<PointComponentSlotId>(-1);
					}
			}
		}

		group.attack_reported_this_tick = false;
		for(UnitAgentState& agent : group.agents)
		{
			if(group.war_active)
			{
				agent.global_state = UnitGlobalState::war;
				agent.substate = UnitSubstate::reconnaissance;
				continue;
			}
			agent.order.clear();
			agent.global_state = UnitGlobalState::rest;
			agent.substate = UnitSubstate::idle;
		}
	}
}
