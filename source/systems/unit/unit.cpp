#include <danikk_space_engine/systems/telemetry/telemetry.h>
#include <danikk_space_engine/systems/unit/unit.h>
#include <danikk_space_engine/systems/unit/internal/battle.h>
#include <danikk_space_engine/systems/unit/internal/mode.h>
#include <danikk_space_engine/systems/unit/internal/orders.h>
#include <danikk_space_engine/systems/unit/internal/perception.h>
#include <danikk_space_engine/systems/unit/internal/rest.h>
#include <danikk_space_engine/systems/unit/internal/state.h>
#include <danikk_space_engine/systems/unit/internal/work.h>
#include <danikk_space_engine/world.h>
#include <danikk_engine/danikk_engine.h>
#include <danikk_space_engine/systems/human/human.h>
#include <danikk_space_engine/systems/human/damageable.h>
#include <danikk_framework/log.h>

#include <danikk_space_engine/world_body.h>
#include <danikk_space_engine/systems/faction/faction.h>
#include <danikk_space_engine/systems/human/system.h>

namespace danikk_space_engine
{
	void UnitSystem::init()
	{
		danikk_framework::formatLogInfo("Init UnitSystem");
		reset();
	}
	void UnitSystem::shutdown()
	{
		danikk_framework::formatLogInfo("Shutdown UnitSystem");
		reset();
	}
	using unit_system_internal::UnitAgentState;
	using unit_system_internal::UnitGroup;
	using unit_system_internal::UnitSighting;

	namespace
	{
		/** @brief Ensures storage contains a tactical group for every configured faction. */
		void ensureUnitGroupCount(DynamicArray<UnitGroup>& groups, size_t faction_count)
		{
			while(groups.size() > faction_count) groups.resize(groups.size() - 1);
			while(groups.size() < faction_count)
			{
				UnitGroup group;
				group.faction_id = static_cast<FactionId>(groups.size());
				groups.push(static_cast<UnitGroup&&>(group));
			}
		}

		/** @brief Removes agent entries whose human was destroyed or replaced. */
		void pruneUnitAgents(World& world, UnitGroup& group)
		{
			for(size_t i = 0; i < group.agents.size();)
			{
				const UnitAgentState& agent = group.agents[i];
				const HumanData* human = world.getHumanSystem().findHuman(agent.human_id);
				if(human != nullptr && human->generation_id == agent.human_generation_id) { ++i; continue; }
				for(size_t j = i + 1; j < group.agents.size(); ++j)
					group.agents[j - 1] = static_cast<UnitAgentState&&>(group.agents[j]);
				group.agents.resize(group.agents.size() - 1);
			}
		}

	}

	/** @brief Resolves a registered live human and its point slot. */
	bool unit_system_internal::getActiveUnit(World& world, UnitAgentState& agent, HumanData*& human)
	{
		if(!agent.enabled) return false;
		HumanData* data = world.getHumanSystem().findHuman(agent.human_id);
		if(data == nullptr || data->generation_id != agent.human_generation_id
			|| !world.getWorldBodyData().isWorldBodyActive(data->world_body_id)) return false;
		human = data;
		if(data->damageable.getHealth() <= 0.0f)
		{
			agent.order.clear();
			return false;
		}
		return true;
	}

	UnitSystem::UnitSystem(World& owner) : world(owner), impl(new Impl()) {}

	UnitSystem::~UnitSystem()
	{
		delete impl;
	}

	/** @brief Shares hostile fire with the threatened faction as fresh attacker intel. */
	void UnitSystem::reportAttack(HumanId attacker_human_id, HumanId victim_human_id)
	{
		DSE_WATCH();
		HumanData* attacker_data = world.getHumanSystem().findHuman(attacker_human_id);
		HumanData* victim_data = world.getHumanSystem().findHuman(victim_human_id);
		if(attacker_data == nullptr || victim_data == nullptr) return;
		HumanState& attacker = *attacker_data;
		HumanState& victim = *victim_data;
		if(attacker.faction_id >= world.getFactionSystem().size() || victim.faction_id >= world.getFactionSystem().size()
			|| world.getFactionSystem().getRelation(victim.faction_id, attacker.faction_id).type != FactionRelationType::hostile)
			return;

		ensureUnitGroupCount(impl->groups, world.getFactionSystem().size());
		UnitGroup& group = impl->groups[victim.faction_id];
		const uint32 now = danikk_engine::getMsTimestamp();
		const glm::dvec3 attacker_position = world.getHumanSystem().getWorldPosition(*attacker_data);
		const glm::dvec3 local_position = attacker_data->body_local_position;
		UnitSighting* contact = nullptr;
		for(UnitSighting& sighting : group.known_sightings)
			if(sighting.target_human_id == attacker_human_id
				&& sighting.target_generation_id == attacker_data->generation_id)
			{
				contact = &sighting;
				break;
			}
		if(contact == nullptr)
		{
			UnitSighting sighting;
			sighting.faction_id = victim.faction_id;
			sighting.target_human_id = attacker_human_id;
			sighting.target_generation_id = attacker_data->generation_id;
			sighting.position = attacker_position;
			sighting.body_id = attacker_data->world_body_id;
			sighting.body_local_position = local_position;
			group.known_sightings.push(sighting);
			contact = &group.known_sightings[group.known_sightings.size() - 1];
		}
		contact->position = attacker_position;
		contact->body_id = attacker_data->world_body_id;
		contact->body_local_position = local_position;
		contact->last_seen_timestamp_ms = now;
		contact->last_attack_timestamp_ms = now;
		contact->last_attacked_human_id = victim_human_id;
		// A confirmed hit is useful tactical intel but does not prove current line of sight.
		group.attack_reported_this_tick = true;
	}

	/** @brief Updates each faction group's perception and orders for the current world tick. */
	void UnitSystem::tick(float dt)
	{
		DSE_WATCH();
		ensureUnitGroupCount(impl->groups, world.getFactionSystem().size());
		unit_system_internal::rebuildUnitSpatialIndex(world, impl->spatial_index);
		for(UnitGroup& group : impl->groups)
		{
			pruneUnitAgents(world, group);
			unit_system_internal::executeUnitOrders(world, group);
			unit_system_internal::updatePerception(world, group, impl->spatial_index);
			unit_system_internal::updateTacticalMemory(world, group);
			unit_system_internal::updateGroupMode(group, dt);
			if(group.war_active)
				unit_system_internal::tickBattle(world, group);
			else
			{
				unit_system_internal::tickWork(world, group);
				unit_system_internal::tickRest(group);
			}
		}
	}

	/** @copydoc UnitSystem::collectNearbyHumanIds */
	void UnitSystem::collectNearbyHumanIds(WorldBodyId body_id, const glm::dvec3& center,
		double half_extent, DynamicArray<HumanId>& result) const
	{
		unit_system_internal::collectNearbyHumanIds(
			world, impl->spatial_index, body_id, center, half_extent, result);
	}

	/** @copydoc UnitSystem::collectHumanIds */
	void UnitSystem::collectHumanIds(DynamicArray<HumanId>& human_ids,
		DynamicArray<WorldBodyId>& body_ids) const
	{
		unit_system_internal::collectHumanIds(impl->spatial_index, human_ids, body_ids);
	}

	/** @brief Registers a human in the tactical group associated with its current faction. */
	void UnitSystem::registerAgent(HumanId human_id, bool enabled)
	{
		DSE_WATCH();
		HumanData* data = world.getHumanSystem().findHuman(human_id);
		if(data == nullptr) return;
		HumanState& human = *data;
		if(human.faction_id >= world.getFactionSystem().size()) return;
		ensureUnitGroupCount(impl->groups, world.getFactionSystem().size());
		UnitGroup& group = impl->groups[human.faction_id];
		for(UnitAgentState& agent : group.agents)
			if(agent.human_id == human_id && agent.human_generation_id == data->generation_id)
			{
				agent.enabled = enabled;
				return;
			}
		UnitAgentState agent;
		agent.human_id = human_id;
		agent.human_generation_id = data->generation_id;
		agent.enabled = enabled;
		group.agents.push(static_cast<UnitAgentState&&>(agent));
	}

	/** @brief Reads the registered unit's current movement target for its order indicator. */
	bool UnitSystem::getMoveTarget(HumanId human_id,
		uint64 generation_id, glm::dvec3& target_world) const
	{
		DSE_WATCH();
		for(const UnitGroup& group : impl->groups)
			for(const UnitAgentState& agent : group.agents)
				if(agent.enabled && agent.human_id == human_id
					&& agent.human_generation_id == generation_id && agent.order.has_move_target)
				{
					target_world = agent.order.move_target_world;
					return true;
				}
		return false;
	}

	/** @brief Reads the current hierarchical state for a registered live unit. */
	bool UnitSystem::getState(HumanId human_id, uint64 generation_id,
		UnitGlobalState& global_state, UnitSubstate& substate) const
	{
		DSE_WATCH();
		for(const UnitGroup& group : impl->groups)
			for(const UnitAgentState& agent : group.agents)
				if(agent.enabled && agent.human_id == human_id
					&& agent.human_generation_id == generation_id)
				{
					global_state = agent.global_state;
					substate = agent.substate;
					return true;
				}
		return false;
	}

	/** @copydoc UnitSystem::reset */
	void UnitSystem::reset()
	{
		impl->groups.clear();
		impl->spatial_index.entries.clear();
	}
}
