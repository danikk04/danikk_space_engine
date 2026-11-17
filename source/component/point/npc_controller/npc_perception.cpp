#include <cmath>

#include <danikk_space_engine/profiler.h>
#include <danikk_space_engine/component/point/human/npc_controller.h>
#include <danikk_space_engine/component/point/human/human.h>
#include <danikk_space_engine/component/point/damageable.h>
#include <danikk_space_engine/component/point/inventory.h>
#include <danikk_space_engine/item.h>
#include <danikk_space_engine/component/world_body/planet/planet.h>
#include <danikk_space_engine/world/world.h>

#include "internal.h"

namespace danikk_space_engine::npc_internal
{
		/** @brief Checks that a candidate still represents a living hostile human. */
		bool validEnemy(World& world, const NpcTarget& target, FactionId faction)
		{
			if(!world.data.isPointComponentSlotActive(target.slot_id)) return false;
			const PointComponentSlot& slot = world.data.getPointComponentSlot(target.slot_id);
			if(slot.type_id != PointComponentTypeId::human || slot.point_id != target.point_id || slot.generation_id != target.generation_id
				|| !world.data.isPointActive(slot.point_id) || !world.data.isChunkActive(slot.chunk_id)) return false;
			const HumanComponent& human = *static_cast<const HumanComponent*>(slot.getData());
			const DamageableComponent* health = findDamageable(world, slot.chunk_id, slot.point_id);
			return health != nullptr && health->getHealth() > 0.0f && faction < world.data.factions.size()
				&& human.faction_id < world.data.factions.size()
				&& world.data.factions.getRelation(faction, human.faction_id).type == FactionRelationType::hostile;
		}

		/** @brief Collects hostile people from the loaded planet chunks around the NPC. */
		void collectCandidates(World& world, PointId point_id, ChunkId chunk_id, FactionId faction,
			NpcControllerComponent& controller)
		{
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
					const NpcTarget target{id, slot.point_id, slot.generation_id, 0.0f};
					if(validEnemy(world, target, faction)) controller.enemy_candidates.push(target);
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


}
