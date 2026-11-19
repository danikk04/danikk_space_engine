#include <danikk_space_engine/profiler.h>
#include <danikk_space_engine/unit_system.h>
#include <danikk_space_engine/world/world.h>
#include <danikk_engine/danikk_engine.h>
#include <danikk_space_engine/component/point/human/human.h>
#include <danikk_space_engine/component/point/damageable.h>

#include "internal.h"

namespace danikk_space_engine
{
	namespace
	{
		/** @brief Holds one independent tactical state object for each faction index. */
		DynamicArray<UnitSystem> unit_systems;

		/** @brief Makes sure the faction state array contains every configured faction. */
		void ensureUnitSystemCount(size_t faction_count)
		{
			while(unit_systems.size() > faction_count)
				unit_systems.resize(unit_systems.size() - 1);
			while(unit_systems.size() < faction_count)
			{
				UnitSystem system;
				system.faction_id = static_cast<FactionId>(unit_systems.size());
				unit_systems.push(static_cast<UnitSystem&&>(system));
			}
		}

		/** @brief Removes agent entries whose pooled human slot was destroyed or reused. */
		void pruneUnitAgents(World& world, UnitSystem& system)
		{
			for(size_t i = 0; i < system.agents.size();)
			{
				const UnitAgentState& agent = system.agents[i];
				if(world.point_data.isPointComponentSlotActive(agent.human_slot_id))
				{
					const PointComponentSlot& slot = world.point_data.getPointComponentSlot(agent.human_slot_id);
					if(slot.type_id == PointComponentTypeId::human
						&& slot.generation_id == agent.human_generation_id)
					{
						++i;
						continue;
					}
				}
				for(size_t j = i + 1; j < system.agents.size(); ++j)
					system.agents[j - 1] = static_cast<UnitAgentState&&>(system.agents[j]);
				system.agents.resize(system.agents.size() - 1);
			}
		}

		/** @brief Finds a living human component attached to a point. */
		PointComponentSlot* findHumanSlot(World& world, PointId point_id, PointComponentSlotId* found_id)
		{
			for(PointComponentSlotId id : world.point_data.getUsedPointComponentSlotIds())
			{
				if(!world.point_data.isPointComponentSlotActive(id)) continue;
				PointComponentSlot& slot = world.point_data.getPointComponentSlot(id);
				if(slot.type_id == PointComponentTypeId::human && slot.point_id == point_id
					&& world.point_data.isPointActive(point_id) && world.chunks.isChunkActive(slot.chunk_id))
				{
					if(found_id != nullptr) *found_id = id;
					return &slot;
				}
			}
			return nullptr;
		}
	}

	/** @brief Shares hostile fire with the threatened faction as fresh attacker intel. */
	void reportUnitAttack(World& world, PointId attacker_point_id, PointId victim_point_id)
	{
		PointComponentSlotId attacker_slot_id = static_cast<PointComponentSlotId>(-1);
		PointComponentSlot* attacker_slot = findHumanSlot(world, attacker_point_id, &attacker_slot_id);
		PointComponentSlot* victim_slot = findHumanSlot(world, victim_point_id, nullptr);
		if(attacker_slot == nullptr || victim_slot == nullptr) return;
		HumanComponent& attacker = *static_cast<HumanComponent*>(attacker_slot->getData());
		HumanComponent& victim = *static_cast<HumanComponent*>(victim_slot->getData());
		if(attacker.faction_id >= world.factions.size() || victim.faction_id >= world.factions.size()
			|| world.factions.getRelation(victim.faction_id, attacker.faction_id).type != FactionRelationType::hostile)
			return;

		ensureUnitSystemCount(world.factions.size());
		UnitSystem& system = unit_systems[victim.faction_id];
		const uint32 now = danikk_engine::getMsTimestamp();
		const Chunk& attacker_chunk = world.chunks.getChunk(attacker_slot->chunk_id);
		const glm::dvec3 attacker_position = world.getPointWorldPosition(attacker_slot->chunk_id, attacker_point_id);
		const glm::dvec3 local_position = attacker_chunk.position
			+ glm::dvec3(world.point_data.getPoint(attacker_point_id).position);
		UnitSighting* contact = nullptr;
		for(UnitSighting& sighting : system.known_sightings)
			if(sighting.target_slot_id == attacker_slot_id
				&& sighting.target_generation_id == attacker_slot->generation_id)
			{
				contact = &sighting;
				break;
			}
		if(contact == nullptr)
		{
			UnitSighting sighting;
			sighting.faction_id = victim.faction_id;
			sighting.target_slot_id = attacker_slot_id;
			sighting.target_point_id = attacker_point_id;
			sighting.target_generation_id = attacker_slot->generation_id;
			sighting.position = attacker_position;
			sighting.body_id = attacker_chunk.world_body_id;
			sighting.body_local_position = local_position;
			system.known_sightings.push(sighting);
			contact = &system.known_sightings[system.known_sightings.size() - 1];
		}
		contact->position = attacker_position;
		contact->body_id = attacker_chunk.world_body_id;
		contact->body_local_position = local_position;
		contact->last_seen_timestamp_ms = now;
		contact->last_attack_timestamp_ms = now;
		contact->last_attacked_point_id = victim_point_id;
		// A confirmed hit is useful faction intel but does not prove current line of sight.
		system.attack_reported_this_tick = true;
	}

	/** @brief Updates each faction's perception and orders for the current world tick. */
	void unitSystemTick(World& world, float dt)
	{
		DSE_WATCH();
		ensureUnitSystemCount(world.factions.size());
		for(UnitSystem& system : unit_systems)
		{
			pruneUnitAgents(world, system);
			unit_system_internal::executeUnitOrders(world, system);
			unit_system_internal::updatePerception(world, system);
			unit_system_internal::assignOrders(world, system);
			unit_system_internal::finishPerception(world, system, dt);
			unit_system_internal::assignSearchOrders(world, system);
			unit_system_internal::assignPostCombatOrders(world, system);
		}
	}

	/** @brief Registers a human in the central AI system associated with its current faction. */
	void registerUnitAgent(World& world, PointComponentSlotId human_slot_id, bool enabled)
	{
		DSE_WATCH();
		if(!world.point_data.isPointComponentSlotActive(human_slot_id)) return;
		PointComponentSlot& slot = world.point_data.getPointComponentSlot(human_slot_id);
		if(slot.type_id != PointComponentTypeId::human) return;
		HumanComponent& human = *static_cast<HumanComponent*>(slot.getData());
		if(human.faction_id >= world.factions.size()) return;
		ensureUnitSystemCount(world.factions.size());
		UnitSystem& system = unit_systems[human.faction_id];
		for(UnitAgentState& agent : system.agents)
			if(agent.human_slot_id == human_slot_id && agent.human_generation_id == slot.generation_id)
			{
				agent.enabled = enabled;
				return;
			}
		UnitAgentState agent;
		agent.human_slot_id = human_slot_id;
		agent.human_generation_id = slot.generation_id;
		agent.enabled = enabled;
		system.agents.push(static_cast<UnitAgentState&&>(agent));
	}

	/** @brief Reads the registered unit's current movement target for its order indicator. */
	bool getUnitMoveTarget(World& world, PointComponentSlotId human_slot_id,
		uint64 generation_id, glm::dvec3& target_world)
	{
		DSE_WATCH();
		for(UnitSystem& system : unit_systems)
			for(UnitAgentState& agent : system.agents)
				if(agent.enabled && agent.human_slot_id == human_slot_id
					&& agent.human_generation_id == generation_id && agent.order.has_move_target)
				{
					target_world = agent.order.move_target_world;
					return true;
				}
		return false;
	}

	/** @brief Reads the current hierarchical state for a registered live unit. */
	bool getUnitState(PointComponentSlotId human_slot_id, uint64 generation_id,
		UnitGlobalState& global_state, UnitSubstate& substate)
	{
		DSE_WATCH();
		for(const UnitSystem& system : unit_systems)
			for(const UnitAgentState& agent : system.agents)
				if(agent.enabled && agent.human_slot_id == human_slot_id
					&& agent.human_generation_id == generation_id)
				{
					global_state = agent.global_state;
					substate = agent.substate;
					return true;
				}
		return false;
	}

	namespace unit_system_internal
	{
		/** @brief Releases all faction-system state when the world is reset or shut down. */
		void resetUnitSystems()
		{
			unit_systems.clear();
		}
	}
}
