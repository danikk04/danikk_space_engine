#include <cmath>

#include <danikk_space_engine/profiler.h>
#include <danikk_space_engine/world/world.h>
#include <danikk_space_engine/component/point/damageable.h>
#include <danikk_space_engine/component/world_body/planet/planet.h>

#include "internal.h"

namespace danikk_space_engine::unit_system_internal
{
	/** @brief Checks that a cached target still identifies a living hostile human. */
	bool isValidEnemy(World& world, const NpcTarget& target, FactionId faction)
	{
		if(!world.data.isPointComponentSlotActive(target.slot_id)) return false;
		const PointComponentSlot& slot = world.data.getPointComponentSlot(target.slot_id);
		if(slot.type_id != PointComponentTypeId::human || slot.point_id != target.point_id
			|| slot.generation_id != target.generation_id || !world.data.isPointActive(slot.point_id)
			|| !world.data.isChunkActive(slot.chunk_id)) return false;
		const HumanComponent& human = *static_cast<const HumanComponent*>(slot.getData());
		const DamageableComponent* health = findDamageable(world, slot.chunk_id, slot.point_id);
		return health != nullptr && health->getHealth() > 0.0f && faction < world.data.factions.size()
			&& human.faction_id < world.data.factions.size()
			&& world.data.factions.getRelation(faction, human.faction_id).type == FactionRelationType::hostile;
	}

	/** @brief Rebuilds a unit's nearby hostile-human candidates from its loaded 3x3x3 chunk neighborhood. */
	void collectEnemyCandidates(World& world, PointId point_id, ChunkId chunk_id, FactionId faction,
		NpcControllerComponent& controller)
	{
		DSE_WATCH();
		controller.enemy_candidates.clear();
		controller.candidate_index = 0;
		const Chunk& owner = world.data.getChunk(chunk_id);
		const WorldBody& body = world.data.getWorldBody(owner.world_body_id);
		const glm::ivec3 coordinate = glm::ivec3(glm::floor(
			(owner.position + glm::dvec3(world.data.getPoint(point_id).position)) / planet_chunk_width));
		const PlanetComponent* planet = nullptr;
		for(WorldBodyComponentSlotId id : body.component_slot_ids)
		{
			if(!world.data.isWorldBodyComponentSlotActive(id)) continue;
			const WorldBodyComponentSlot& slot = world.data.getWorldBodyComponentSlot(id);
			if(slot.type_id == WorldBodyComponentTypeId::planet)
			{
				planet = static_cast<const PlanetComponent*>(slot.getData());
				break;
			}
		}
		auto collect = [&](ChunkId candidate_chunk)
		{
			if(!world.data.isChunkActive(candidate_chunk)) return;
			for(PointComponentSlotId id : world.data.getChunk(candidate_chunk).point_component_slot_ids)
			{
				if(!world.data.isPointComponentSlotActive(id)) continue;
				const PointComponentSlot& slot = world.data.getPointComponentSlot(id);
				if(slot.type_id != PointComponentTypeId::human || slot.point_id == point_id) continue;
				const NpcTarget target{id, slot.point_id, slot.generation_id};
				if(isValidEnemy(world, target, faction)) controller.enemy_candidates.push(target);
			}
		};
		if(planet == nullptr) { collect(chunk_id); return; }
		for(int x = -1; x <= 1; ++x)
			for(int y = -1; y <= 1; ++y)
				for(int z = -1; z <= 1; ++z)
				{
					ChunkId nearby = static_cast<ChunkId>(-1);
					if(planet->chunk_lookup.getChunk(coordinate + glm::ivec3(x, y, z), nearby)) collect(nearby);
				}
	}

	/** @brief Checks that a shared contact still names a living hostile human. */
	bool isValidSighting(World& world, const UnitSighting& sighting)
	{
		if(sighting.faction_id >= world.data.factions.size()
			|| !world.data.isPointComponentSlotActive(sighting.target_slot_id)) return false;
		const PointComponentSlot& target_slot = world.data.getPointComponentSlot(sighting.target_slot_id);
		if(target_slot.type_id != PointComponentTypeId::human
			|| target_slot.point_id != sighting.target_point_id
			|| target_slot.generation_id != sighting.target_generation_id
			|| !world.data.isPointActive(target_slot.point_id)
			|| !world.data.isChunkActive(target_slot.chunk_id)) return false;
		const HumanComponent& target = *static_cast<const HumanComponent*>(target_slot.getData());
		if(target.faction_id >= world.data.factions.size()
			|| world.data.factions.getRelation(sighting.faction_id, target.faction_id).type != FactionRelationType::hostile)
			return false;
		const DamageableComponent* health = findDamageable(world, target_slot.chunk_id, target_slot.point_id);
		return health != nullptr && health->getHealth() > 0.0f;
	}

	/** @brief Refreshes an existing faction contact or appends a newly observed target. */
	void rememberSighting(DynamicArray<UnitSighting>& sightings, FactionId faction_id,
		const NpcTarget& target, const glm::dvec3& position)
	{
		for(UnitSighting& sighting : sightings)
		{
			if(sighting.faction_id != faction_id || sighting.target_slot_id != target.slot_id
				|| sighting.target_generation_id != target.generation_id) continue;
			sighting.target_point_id = target.point_id;
			sighting.position = position;
			sighting.age = 0.0f;
			return;
		}
		UnitSighting sighting;
		sighting.faction_id = faction_id;
		sighting.target_slot_id = target.slot_id;
		sighting.target_point_id = target.point_id;
		sighting.target_generation_id = target.generation_id;
		sighting.position = position;
		sightings.push(sighting);
	}

	/** @brief Removes one expired contact while preserving the remaining order. */
	void removeSighting(DynamicArray<UnitSighting>& sightings, size_t index)
	{
		for(size_t i = index + 1; i < sightings.size(); ++i)
			sightings[i - 1] = static_cast<UnitSighting&&>(sightings[i]);
		sightings.resize(sightings.size() - 1);
	}

	/** @brief Ages and prunes faction intelligence, then performs the scheduled NPC visibility raycasts. */
	void updatePerception(World& world, UnitSystem& system,
		const DynamicArray<PointComponentSlotId>& slot_ids, float dt)
	{
		DynamicArray<UnitSighting>& known_sightings = system.known_sightings;
		for(UnitSighting& sighting : known_sightings) sighting.age += dt;
		for(size_t i = 0; i < known_sightings.size();)
		{
			if(known_sightings[i].age > 8.0f || !isValidSighting(world, known_sightings[i]))
			{
				removeSighting(known_sightings, i);
				continue;
			}
			++i;
		}

		for(PointComponentSlotId id : slot_ids)
		{
			PointComponentSlot* controller_slot = nullptr;
			NpcControllerComponent* controller = nullptr;
			HumanComponent* human = nullptr;
			if(!getActiveNpc(world, id, controller_slot, controller, human)) continue;
			if(human->faction_id != system.faction_id) continue;
			const NpcTarget previous_target{controller->order.target_slot_id,
				controller->order.target_point_id, controller->order.target_generation_id};
			if(controller->order.has_attack_target && isValidEnemy(world, previous_target, human->faction_id))
			{
				const PointComponentSlot& target_slot = world.data.getPointComponentSlot(previous_target.slot_id);
				const glm::dvec3 origin = world.getPointWorldPosition(controller_slot->chunk_id, controller_slot->point_id);
				const Chunk& chunk = world.data.getChunk(controller_slot->chunk_id);
				const WorldBody& body = world.data.getWorldBody(chunk.world_body_id);
				const glm::dvec3 shot_origin = origin + body.rotation
					* glm::dvec3(human->up * (human->getHeight() * 0.2f));
				const glm::dvec3 target_position = world.getPointWorldPosition(target_slot.chunk_id, previous_target.point_id);
				const glm::dvec3 delta = target_position - shot_origin;
				const double distance = glm::length(delta);
				if(distance > 0.001)
				{
					const RaycastHit hit = world.raycast(shot_origin, vec3(delta), static_cast<float>(distance));
					if(!hit.hit || hit.distance >= distance - 0.1)
						rememberSighting(known_sightings, human->faction_id, previous_target, target_position);
				}
			}
			else
			{
				controller->order.has_attack_target = false;
				if(controller->candidate_index >= controller->enemy_candidates.size())
					collectEnemyCandidates(world, controller_slot->point_id,
						controller_slot->chunk_id, human->faction_id, *controller);
				if(controller->candidate_index >= controller->enemy_candidates.size()) continue;
				const NpcTarget candidate = controller->enemy_candidates[controller->candidate_index++];
				if(!isValidEnemy(world, candidate, human->faction_id)) continue;
				const PointComponentSlot& target_slot = world.data.getPointComponentSlot(candidate.slot_id);
				const glm::dvec3 origin = world.getPointWorldPosition(controller_slot->chunk_id, controller_slot->point_id);
				const Chunk& chunk = world.data.getChunk(controller_slot->chunk_id);
				const WorldBody& body = world.data.getWorldBody(chunk.world_body_id);
				const glm::dvec3 shot_origin = origin + body.rotation
					* glm::dvec3(human->up * (human->getHeight() * 0.2f));
				const glm::dvec3 target_position = world.getPointWorldPosition(target_slot.chunk_id, candidate.point_id);
				const glm::dvec3 delta = target_position - shot_origin;
				const double distance = glm::length(delta);
				if(distance <= 0.001) continue;
				const RaycastHit hit = world.raycast(shot_origin, vec3(delta), static_cast<float>(distance));
				if(!hit.hit || hit.distance >= distance - 0.1)
					rememberSighting(known_sightings, human->faction_id, candidate, target_position);
			}
		}
	}
}
