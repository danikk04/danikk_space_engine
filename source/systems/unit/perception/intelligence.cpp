#include <danikk_engine/danikk_engine.h>
#include <danikk_space_engine/systems/telemetry/telemetry.h>
#include <danikk_space_engine/world.h>
#include <danikk_space_engine/systems/human/damageable.h>

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
				if(location.target_human_id != sighting.target_human_id
					|| location.target_generation_id != sighting.target_generation_id) continue;
				location.body_id = sighting.body_id;
				location.body_local_position = sighting.body_local_position;
				location.is_corpse = is_corpse;
				return;
			}
			UnitSearchLocation location;
			location.body_id = sighting.body_id;
			location.body_local_position = sighting.body_local_position;
			location.target_human_id = sighting.target_human_id;
			location.target_generation_id = sighting.target_generation_id;
			location.is_corpse = is_corpse;
			system.search_locations.push(location);
		}
		/** @brief Adds known dead faction members to the same post-combat inspection queue as lost contacts. */
		void rememberFriendlyCorpseLocations(World& world, UnitGroup& system)
		{
			for(const HumanData& human : world.humans.getHumans())
			{
				if(human.faction_id != system.faction_id || human.damageable.getHealth() > 0.0f
					|| !world.world_body_data.isWorldBodyActive(human.world_body_id)) continue;

				UnitSighting corpse_location;
				corpse_location.faction_id = system.faction_id;
				corpse_location.target_human_id = human.id;
				corpse_location.target_generation_id = human.generation_id;
				corpse_location.body_id = human.world_body_id;
				corpse_location.body_local_position = human.body_local_position;
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
		const HumanData* human = world.humans.findHuman(target.human_id);
		if(human == nullptr || human->generation_id != target.generation_id
			|| !world.world_body_data.isWorldBodyActive(human->world_body_id)) return false;
		return human->damageable.getHealth() > 0.0f && faction < world.factions.size()
			&& human->faction_id < world.factions.size()
			&& world.factions.getRelation(faction, human->faction_id).type == FactionRelationType::hostile;
	}
	/** @brief Checks that a shared contact still names a living hostile human. */
	bool isValidSighting(World& world, const UnitSighting& sighting)
	{
		if(sighting.faction_id >= world.factions.size()) return false;
		const HumanData* target = world.humans.findHuman(sighting.target_human_id);
		if(target == nullptr || target->generation_id != sighting.target_generation_id
			|| !world.world_body_data.isWorldBodyActive(target->world_body_id)) return false;
		if(target->faction_id >= world.factions.size()
			|| world.factions.getRelation(sighting.faction_id, target->faction_id).type != FactionRelationType::hostile)
			return false;
		return target->damageable.getHealth() > 0.0f;
	}

	/** @brief Refreshes an existing faction contact or appends a newly observed target. */
	void rememberSighting(World& world, UnitGroup& system, FactionId faction_id,
		const UnitTarget& target, const glm::dvec3& position)
	{
		const HumanData* target_data = world.humans.findHuman(target.human_id);
		if(target_data == nullptr || target_data->generation_id != target.generation_id
			|| !world.world_body_data.isWorldBodyActive(target_data->world_body_id)) return;
		const glm::dvec3 body_local_position = target_data->body_local_position;
		for(UnitSighting& sighting : system.known_sightings)
		{
			if(sighting.faction_id != faction_id || sighting.target_human_id != target.human_id
				|| sighting.target_generation_id != target.generation_id) continue;
			sighting.position = position;
			sighting.body_id = target_data->world_body_id;
			sighting.body_local_position = body_local_position;
			sighting.last_seen_timestamp_ms = danikk_engine::getMsTimestamp();
			sighting.visible_this_tick = true;
			sighting.visibility_checked_this_tick = true;
			system.enemy_seen_this_tick = true;
			return;
		}
		UnitSighting sighting;
		sighting.faction_id = faction_id;
		sighting.target_human_id = target.human_id;
		sighting.target_generation_id = target.generation_id;
		sighting.position = position;
		sighting.body_id = target_data->world_body_id;
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
