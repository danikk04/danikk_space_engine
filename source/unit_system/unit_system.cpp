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

		/** @brief Finds a living human component attached to a point. */
		PointComponentSlot* findHumanSlot(World& world, PointId point_id, PointComponentSlotId* found_id)
		{
			for(PointComponentSlotId id : world.data.getUsedPointComponentSlotIds())
			{
				if(!world.data.isPointComponentSlotActive(id)) continue;
				PointComponentSlot& slot = world.data.getPointComponentSlot(id);
				if(slot.type_id == PointComponentTypeId::human && slot.point_id == point_id
					&& world.data.isPointActive(point_id) && world.data.isChunkActive(slot.chunk_id))
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
		if(attacker.faction_id >= world.data.factions.size() || victim.faction_id >= world.data.factions.size()
			|| world.data.factions.getRelation(victim.faction_id, attacker.faction_id).type != FactionRelationType::hostile)
			return;

		ensureUnitSystemCount(world.data.factions.size());
		UnitSystem& system = unit_systems[victim.faction_id];
		const uint32 now = danikk_engine::getMsTimestamp();
		const Chunk& attacker_chunk = world.data.getChunk(attacker_slot->chunk_id);
		const glm::dvec3 attacker_position = world.getPointWorldPosition(attacker_slot->chunk_id, attacker_point_id);
		const glm::dvec3 local_position = attacker_chunk.position
			+ glm::dvec3(world.data.getPoint(attacker_point_id).position);
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
		ensureUnitSystemCount(world.data.factions.size());

		const DynamicArray<PointComponentSlotId>& slot_ids = world.data.getUsedPointComponentSlotIds();
		for(UnitSystem& system : unit_systems)
		{
			unit_system_internal::updatePerception(world, system, slot_ids);
			unit_system_internal::assignOrders(world, system, slot_ids);
			unit_system_internal::finishPerception(world, system, dt);
			unit_system_internal::assignSearchOrders(world, system, slot_ids);
		}
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
