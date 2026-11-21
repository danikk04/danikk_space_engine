#include <danikk_space_engine/profiler.h>
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
#include <danikk_space_engine/component/point/human/human.h>
#include <danikk_space_engine/component/point/damageable.h>
#include <danikk_framework/log.h>

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

		/** @brief Removes agent entries whose pooled human slot was destroyed or reused. */
		void pruneUnitAgents(World& world, UnitGroup& group)
		{
			for(size_t i = 0; i < group.agents.size();)
			{
				const UnitAgentState& agent = group.agents[i];
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
				for(size_t j = i + 1; j < group.agents.size(); ++j)
					group.agents[j - 1] = static_cast<UnitAgentState&&>(group.agents[j]);
				group.agents.resize(group.agents.size() - 1);
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

	/** @brief Resolves a registered live human and its point slot. */
	bool unit_system_internal::getActiveUnit(World& world, UnitAgentState& agent,
		PointComponentSlot*& human_slot, HumanComponent*& human)
	{
		if(!agent.enabled || !world.point_data.isPointComponentSlotActive(agent.human_slot_id)) return false;
		human_slot = &world.point_data.getPointComponentSlot(agent.human_slot_id);
		if(human_slot->type_id != PointComponentTypeId::human
			|| human_slot->generation_id != agent.human_generation_id
			|| !world.point_data.isPointActive(human_slot->point_id)
			|| !world.chunks.isChunkActive(human_slot->chunk_id)) return false;
		human = static_cast<HumanComponent*>(human_slot->getData());
		const DamageableComponent* health = findDamageable(world, human_slot->point_id);
		if(health == nullptr || health->getHealth() <= 0.0f)
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
	void UnitSystem::reportAttack(PointId attacker_point_id, PointId victim_point_id)
	{
		DSE_WATCH();
		PointComponentSlotId attacker_slot_id = static_cast<PointComponentSlotId>(-1);
		PointComponentSlot* attacker_slot = findHumanSlot(world, attacker_point_id, &attacker_slot_id);
		PointComponentSlot* victim_slot = findHumanSlot(world, victim_point_id, nullptr);
		if(attacker_slot == nullptr || victim_slot == nullptr) return;
		HumanComponent& attacker = *static_cast<HumanComponent*>(attacker_slot->getData());
		HumanComponent& victim = *static_cast<HumanComponent*>(victim_slot->getData());
		if(attacker.faction_id >= world.factions.size() || victim.faction_id >= world.factions.size()
			|| world.factions.getRelation(victim.faction_id, attacker.faction_id).type != FactionRelationType::hostile)
			return;

		ensureUnitGroupCount(impl->groups, world.factions.size());
		UnitGroup& group = impl->groups[victim.faction_id];
		const uint32 now = danikk_engine::getMsTimestamp();
		const Chunk& attacker_chunk = world.chunks.getChunk(attacker_slot->chunk_id);
		const glm::dvec3 attacker_position = world.getPointWorldPosition(attacker_slot->chunk_id, attacker_point_id);
		const glm::dvec3 local_position = attacker_chunk.position
			+ glm::dvec3(world.point_data.getPoint(attacker_point_id).position);
		UnitSighting* contact = nullptr;
		for(UnitSighting& sighting : group.known_sightings)
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
			group.known_sightings.push(sighting);
			contact = &group.known_sightings[group.known_sightings.size() - 1];
		}
		contact->position = attacker_position;
		contact->body_id = attacker_chunk.world_body_id;
		contact->body_local_position = local_position;
		contact->last_seen_timestamp_ms = now;
		contact->last_attack_timestamp_ms = now;
		contact->last_attacked_point_id = victim_point_id;
		// A confirmed hit is useful tactical intel but does not prove current line of sight.
		group.attack_reported_this_tick = true;
	}

	/** @brief Updates each faction group's perception and orders for the current world tick. */
	void UnitSystem::tick(float dt)
	{
		DSE_WATCH();
		ensureUnitGroupCount(impl->groups, world.factions.size());
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
		unit_system_internal::rebuildUnitSpatialIndex(world, impl->spatial_index);
	}

	/** @copydoc UnitSystem::collectNearbyHumanSlots */
	void UnitSystem::collectNearbyHumanSlots(WorldBodyId body_id, const glm::dvec3& center,
		double half_extent, DynamicArray<PointComponentSlotId>& result) const
	{
		unit_system_internal::collectNearbyHumanSlots(
			impl->spatial_index, body_id, center, half_extent, result);
	}

	/** @brief Registers a human in the tactical group associated with its current faction. */
	void UnitSystem::registerAgent(PointComponentSlotId human_slot_id, bool enabled)
	{
		DSE_WATCH();
		if(!world.point_data.isPointComponentSlotActive(human_slot_id)) return;
		PointComponentSlot& slot = world.point_data.getPointComponentSlot(human_slot_id);
		if(slot.type_id != PointComponentTypeId::human) return;
		HumanComponent& human = *static_cast<HumanComponent*>(slot.getData());
		if(human.faction_id >= world.factions.size()) return;
		ensureUnitGroupCount(impl->groups, world.factions.size());
		UnitGroup& group = impl->groups[human.faction_id];
		for(UnitAgentState& agent : group.agents)
			if(agent.human_slot_id == human_slot_id && agent.human_generation_id == slot.generation_id)
			{
				agent.enabled = enabled;
				return;
			}
		UnitAgentState agent;
		agent.human_slot_id = human_slot_id;
		agent.human_generation_id = slot.generation_id;
		agent.enabled = enabled;
		group.agents.push(static_cast<UnitAgentState&&>(agent));
	}

	/** @brief Reads the registered unit's current movement target for its order indicator. */
	bool UnitSystem::getMoveTarget(PointComponentSlotId human_slot_id,
		uint64 generation_id, glm::dvec3& target_world) const
	{
		DSE_WATCH();
		for(const UnitGroup& group : impl->groups)
			for(const UnitAgentState& agent : group.agents)
				if(agent.enabled && agent.human_slot_id == human_slot_id
					&& agent.human_generation_id == generation_id && agent.order.has_move_target)
				{
					target_world = agent.order.move_target_world;
					return true;
				}
		return false;
	}

	/** @brief Reads the current hierarchical state for a registered live unit. */
	bool UnitSystem::getState(PointComponentSlotId human_slot_id, uint64 generation_id,
		UnitGlobalState& global_state, UnitSubstate& substate) const
	{
		DSE_WATCH();
		for(const UnitGroup& group : impl->groups)
			for(const UnitAgentState& agent : group.agents)
				if(agent.enabled && agent.human_slot_id == human_slot_id
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
		impl->spatial_index.trees.clear();
	}
}
