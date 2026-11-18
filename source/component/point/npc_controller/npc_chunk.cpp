#include <cmath>

#include <danikk_space_engine/profiler.h>
#include <danikk_space_engine/component/point/human/npc_controller.h>
#include <danikk_space_engine/component/point/human/human.h>
#include <danikk_space_engine/component/point/damageable.h>
#include <danikk_space_engine/component/point/inventory.h>
#include <danikk_space_engine/item/item.h>
#include <danikk_space_engine/component/world_body/planet/planet.h>
#include <danikk_space_engine/world/world.h>

#include "internal.h"

namespace danikk_space_engine::npc_internal
{
		constexpr bool log_npc_chunk_transitions = false;
		/** @brief Moves an NPC and its point components to the loaded planet chunk containing its position. */
		void updateNpcChunk(World& world, PointId point_id, ChunkId old_chunk_id, NpcControllerComponent& controller)
		{
			Chunk& old_chunk = world.data.getChunk(old_chunk_id);
			WorldBody& body = world.data.getWorldBody(old_chunk.world_body_id);
			const glm::dvec3 position = old_chunk.position + glm::dvec3(world.data.getPoint(point_id).position);
			const glm::ivec3 coordinate = glm::ivec3(glm::floor(position / planet_chunk_width));
			ChunkId new_chunk_id = static_cast<ChunkId>(-1);
			bool is_planet = false;
			for(WorldBodyComponentSlotId id : body.component_slot_ids)
			{
				if(!world.data.isWorldBodyComponentSlotActive(id)) continue;
				WorldBodyComponentSlot& component = world.data.getWorldBodyComponentSlot(id);
				if(component.type_id != WorldBodyComponentTypeId::planet) continue;
				is_planet = true;
				PlanetComponent& planet = *static_cast<PlanetComponent*>(component.getData());
				planet.chunk_lookup.getChunk(coordinate, new_chunk_id);
				break;
			}
			if(!is_planet || new_chunk_id == old_chunk_id) return;
			if(!world.data.isChunkActive(new_chunk_id))
			{
				const glm::dvec3 minimum = glm::dvec3(old_chunk.grid_coordinate) * planet_chunk_width;
				const glm::dvec3 maximum = minimum + glm::dvec3(planet_chunk_width - 0.001);
				world.data.getPoint(point_id).position = vec3(glm::clamp(position, minimum, maximum) - old_chunk.position);
				return;
			}
			const glm::dvec3 new_origin = world.data.getChunk(new_chunk_id).position;
			DynamicArray<PointComponentSlotId> moved;
			for(size_t i = 0; i < old_chunk.point_component_slot_ids.size();)
			{
				const PointComponentSlotId id = old_chunk.point_component_slot_ids[i];
				if(world.data.getPointComponentSlot(id).point_id != point_id) { ++i; continue; }
				moved.push(id);
				world.data.removePointComponentSlotFromChunk(id);
			}
			for(size_t i = 0; i < old_chunk.point_ids.size(); ++i)
			{
				if(old_chunk.point_ids[i] != point_id) continue;
				for(size_t j = i; j + 1 < old_chunk.point_ids.size(); ++j) old_chunk.point_ids[j] = old_chunk.point_ids[j + 1];
				old_chunk.point_ids.resize(old_chunk.point_ids.size() - 1);
				world.data.getPoint(point_id).referencing_chunk_count--;
				break;
			}
			world.data.getPoint(point_id).position = vec3(position - new_origin);
			for(PointComponentSlotId id : moved) world.data.addPointComponentSlotToChunk(new_chunk_id, id);
			if constexpr(log_npc_chunk_transitions)
			{
				const glm::ivec3 old_coordinate = old_chunk.grid_coordinate;
				formatLogInfo("NPC point % moved from chunk % (%, %, %) to chunk % (%, %, %)",
					point_id, old_chunk_id, old_coordinate.x, old_coordinate.y, old_coordinate.z,
					new_chunk_id, coordinate.x, coordinate.y, coordinate.z);
			}
		}


}
