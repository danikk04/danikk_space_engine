#include <danikk_space_engine/profiler.h>
#include <danikk_space_engine/world/world.h>
#include <danikk_space_engine/component_tick_system.h>

namespace danikk_space_engine
{
	/** @copydoc ComponentSystem::tickChunkComponents */
	void ComponentSystem::tickChunkComponents(World& world, ChunkId chunk_id)
	{
		DSE_WATCH();
		if(!world.chunk_system.isChunkActive(chunk_id)) return;

		Chunk& chunk = world.chunk_system.getChunk(chunk_id);
		const size_t component_count = chunk.point_component_slot_ids.size();
		for(size_t component_index = 0; component_index < component_count; component_index++)
		{
			const PointComponentSlotId slot_id = chunk.point_component_slot_ids[component_index];
			if(slot_id >= world.data.getPointComponentSlotCount())
			{
				danikk_engine::formatFatalError("Chunk references invalid point component slot %", slot_id);
			}
			if(!world.data.isPointComponentSlotActive(slot_id)) continue;

			PointComponentSlot& slot = world.data.getPointComponentSlot(slot_id);
			if(slot.chunk_id != chunk_id) continue;
			const PointComponentType& type = getPointComponentType(slot.type_id);
			if(slot.uses_external_data && slot.external_data == nullptr)
			{
				danikk_engine::formatFatalError("Point component slot % has no external data", slot_id);
			}
			if(type.tick != nullptr) type.tick(slot, world);
		}
	}

	/** @copydoc ComponentSystem::tickWorldBodyComponents */
	void ComponentSystem::tickWorldBodyComponents(World& world)
	{
		DSE_WATCH();
		const DynamicArray<WorldBodyId> world_body_ids(world.data.getUsedWorldBodyIds());
		DynamicArray<WorldBodyComponentSlotId> component_slot_ids;
		DynamicArray<ChunkId> chunk_ids;

		for(WorldBodyId body_id : world_body_ids)
		{
			if(!world.data.isWorldBodyActive(body_id)) continue;
			component_slot_ids.clear();
			chunk_ids.clear();
			{
				const WorldBody& body = world.data.getWorldBody(body_id);
				for(WorldBodyComponentSlotId slot_id : body.component_slot_ids)
					component_slot_ids.push(slot_id);
				for(ChunkId chunk_id : body.chunk_ids)
					chunk_ids.push(chunk_id);
			}

			for(WorldBodyComponentSlotId slot_id : component_slot_ids)
			{
				if(!world.data.isWorldBodyComponentSlotActive(slot_id)) continue;
				WorldBodyComponentSlot& slot = world.data.getWorldBodyComponentSlot(slot_id);
				const WorldBodyComponentType& type = getWorldBodyComponentType(slot.type_id);
				if(type.tick != nullptr) type.tick(slot, world);
			}
			for(ChunkId chunk_id : chunk_ids)
			{
				if(world.chunk_system.isChunkActive(chunk_id)) tickChunkComponents(world, chunk_id);
			}
		}
	}
}
