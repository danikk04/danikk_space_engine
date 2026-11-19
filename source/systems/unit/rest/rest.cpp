#include <danikk_space_engine/profiler.h>

#include <danikk_space_engine/systems/unit/internal/rest.h>
#include <danikk_space_engine/systems/unit/internal/state.h>

namespace danikk_space_engine::unit_system_internal
{
	/** @brief Keeps agents without an assigned work task in the idle rest state. */
	void tickRest(UnitGroup& group)
	{
		DSE_WATCH();
		for(UnitAgentState& agent : group.agents)
		{
			if(agent.global_state == UnitGlobalState::work) continue;
			agent.order.clear();
			agent.global_state = UnitGlobalState::rest;
			agent.substate = UnitSubstate::idle;
		}
	}
}
