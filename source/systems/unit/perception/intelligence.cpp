#include <danikk_engine/danikk_engine.h>
#include <danikk_space_engine/profiler.h>
#include <danikk_space_engine/world.h>
#include <danikk_space_engine/component/point/damageable.h>

#include <danikk_space_engine/systems/unit/internal/perception.h>
#include <danikk_space_engine/systems/unit/internal/state.h>

namespace danikk_space_engine::unit_system_internal
{
	static constexpr uint32 sighting_timeout_ms = 8000;

		/** @brief Adds or refreshes the investigation location associated with a lost target. */
		void rememberSearchLocation(UnitGroup& system, const UnitSighting& sighting, bool is_corpse = false)
		{
			for(UnitSearchLocation& location : system.search_locations)
			{
				if(location.target_slot_id != sighting.target_slot_id
					|| location.target_generation_id != sighting.target_generation_id) continue;
				location.body_id = sighting.body_id;
				location.body_local_position = sighting.body_local_position;
				location.is_corpse = is_corpse;
				return;
			}
			UnitSearchLocation location;
			location.body_id = sighting.body_id;
			location.body_local_position = sighting.body_local_position;
			location.target_slot_id = sighting.target_slot_id;
			location.target_generation_id = sighting.target_generation_id;
			location.is_corpse = is_corpse;
			system.search_locations.push(location);
		}
		/** @brief Adds known dead faction members to the same post-combat inspection queue as lost contacts. */
		void rememberFriendlyCorpseLocations(World& world, UnitGroup& system)
		{
			for(PointComponentSlotId id : world.point_data.getUsedPointComponentSlotIds())
			{
				if(!world.point_data.isPointComponentSlotActive(id)) continue;
				const PointComponentSlot& slot = world.point_data.getPointComponentSlot(id);
				if(slot.type_id != PointComponentTypeId::human || !world.point_data.isPointActive(slot.point_id)
					|| !world.chunks.isChunkActive(slot.chunk_id)) continue;
				const HumanComponent& human = *static_cast<const HumanComponent*>(slot.getData());
				if(human.faction_id != system.faction_id) continue;
				const DamageableComponent* health = findDamageable(world, slot.chunk_id, slot.point_id);
				if(health == nullptr || health->getHealth() > 0.0f) continue;

				const Chunk& chunk = world.chunks.getChunk(slot.chunk_id);
				UnitSighting corpse_location;
				corpse_location.faction_id = system.faction_id;
				corpse_location.target_slot_id = id;
				corpse_location.target_generation_id = slot.generation_id;
				corpse_location.body_id = chunk.world_body_id;
				corpse_location.body_local_position = chunk.position
					+ glm::dvec3(world.point_data.getPoint(slot.point_id).position);
				rememberSearchLocation(system, corpse_location, true);
			}
		}
	/** @brief Checks whether a sighting timestamp is within the eight-second tactical memory window. */
	bool isRecentSighting(const UnitSighting& sighting)
	{
		return sighting.last_seen_timestamp_ms != 0
			&& danikk_engine::getMsTimestamp() - sighting.last_seen_timestamp_ms <= sighting_timeout_ms;
	}

	/** @brief Checks that a cached target still identifies a living hostile human. */
	bool isValidEnemy(World& world, const UnitTarget& target, FactionId faction)
	{
		if(!world.point_data.isPointComponentSlotActive(target.slot_id)) return false;
		const PointComponentSlot& slot = world.point_data.getPointComponentSlot(target.slot_id);
		if(slot.type_id != PointComponentTypeId::human || slot.point_id != target.point_id
			|| slot.generation_id != target.generation_id || !world.point_data.isPointActive(slot.point_id)
			|| !world.chunks.isChunkActive(slot.chunk_id)) return false;
		const HumanComponent& human = *static_cast<const HumanComponent*>(slot.getData());
		const DamageableComponent* health = findDamageable(world, slot.chunk_id, slot.point_id);
		return health != nullptr && health->getHealth() > 0.0f && faction < world.factions.size()
			&& human.faction_id < world.factions.size()
			&& world.factions.getRelation(faction, human.faction_id).type == FactionRelationType::hostile;
	}
	/** @brief Checks that a shared contact still names a living hostile human. */
	bool isValidSighting(World& world, const UnitSighting& sighting)
	{
		if(sighting.faction_id >= world.factions.size()
			|| !world.point_data.isPointComponentSlotActive(sighting.target_slot_id)) return false;
		const PointComponentSlot& target_slot = world.point_data.getPointComponentSlot(sighting.target_slot_id);
		if(target_slot.type_id != PointComponentTypeId::human
			|| target_slot.point_id != sighting.target_point_id
			|| target_slot.generation_id != sighting.target_generation_id
			|| !world.point_data.isPointActive(target_slot.point_id)
			|| !world.chunks.isChunkActive(target_slot.chunk_id)) return false;
		const HumanComponent& target = *static_cast<const HumanComponent*>(target_slot.getData());
		if(target.faction_id >= world.factions.size()
			|| world.factions.getRelation(sighting.faction_id, target.faction_id).type != FactionRelationType::hostile)
			return false;
		const DamageableComponent* health = findDamageable(world, target_slot.chunk_id, target_slot.point_id);
		return health != nullptr && health->getHealth() > 0.0f;
	}

	/** @brief Refreshes an existing faction contact or appends a newly observed target. */
	void rememberSighting(World& world, UnitGroup& system, FactionId faction_id,
		const UnitTarget& target, const glm::dvec3& position)
	{
		if(!world.point_data.isPointComponentSlotActive(target.slot_id)) return;
		const PointComponentSlot& target_slot = world.point_data.getPointComponentSlot(target.slot_id);
		if(!world.chunks.isChunkActive(target_slot.chunk_id) || !world.point_data.isPointActive(target.point_id)) return;
		const Chunk& target_chunk = world.chunks.getChunk(target_slot.chunk_id);
		const glm::dvec3 body_local_position = target_chunk.position
			+ glm::dvec3(world.point_data.getPoint(target.point_id).position);
		for(UnitSighting& sighting : system.known_sightings)
		{
			if(sighting.faction_id != faction_id || sighting.target_slot_id != target.slot_id
				|| sighting.target_generation_id != target.generation_id) continue;
			sighting.target_point_id = target.point_id;
			sighting.position = position;
			sighting.body_id = target_chunk.world_body_id;
			sighting.body_local_position = body_local_position;
			sighting.last_seen_timestamp_ms = danikk_engine::getMsTimestamp();
			sighting.visible_this_tick = true;
			sighting.visibility_checked_this_tick = true;
			system.enemy_seen_this_tick = true;
			return;
		}
		UnitSighting sighting;
		sighting.faction_id = faction_id;
		sighting.target_slot_id = target.slot_id;
		sighting.target_point_id = target.point_id;
		sighting.target_generation_id = target.generation_id;
		sighting.position = position;
		sighting.body_id = target_chunk.world_body_id;
		sighting.body_local_position = body_local_position;
		sighting.last_seen_timestamp_ms = danikk_engine::getMsTimestamp();
		sighting.visible_this_tick = true;
		sighting.visibility_checked_this_tick = true;
		system.enemy_seen_this_tick = true;
		system.known_sightings.push(sighting);
	}

	/** @brief Removes one expired contact while preserving the remaining order. */
	void removeSighting(DynamicArray<UnitSighting>& sightings, size_t index)
	{
		for(size_t i = index + 1; i < sightings.size(); ++i)
			sightings[i - 1] = static_cast<UnitSighting&&>(sightings[i]);
		sightings.resize(sightings.size() - 1);
	}
	/** @brief Records contacts that were lost, expires stale intelligence, and tracks visible corpses. */
	void updateTacticalMemory(World& world, UnitGroup& system)
	{
		DSE_WATCH();
		for(size_t index = 0; index < system.known_sightings.size();)
		{
			UnitSighting& sighting = system.known_sightings[index];
			if(sighting.visible_this_tick) sighting.visible_last_tick = true;
			else if(sighting.visibility_checked_this_tick && sighting.visible_last_tick)
			{
				rememberSearchLocation(system, sighting);
				sighting.visible_last_tick = false;
			}

			if(!isRecentSighting(sighting) || !isValidSighting(world, sighting))
			{
				removeSighting(system.known_sightings, index);
				continue;
			}
			++index;
		}
		rememberFriendlyCorpseLocations(world, system);

		for(size_t index = 0; index < system.search_locations.size();)
		{
			if(!world.world_body_data.isWorldBodyActive(system.search_locations[index].body_id))
			{
				for(size_t shift = index + 1; shift < system.search_locations.size(); ++shift)
					system.search_locations[shift - 1] = static_cast<UnitSearchLocation&&>(system.search_locations[shift]);
				system.search_locations.resize(system.search_locations.size() - 1);
				continue;
			}
			++index;
		}
	}
}
