#include <danikk_space_engine/profiler.h>
#include <danikk_space_engine/world.h>
#include <danikk_space_engine/component_system.h>

namespace danikk_space_engine
{
	/** @copydoc ComponentSystem::tickChunkComponents */
	void ComponentSystem::tickChunkComponents(ChunkId chunk_id)
	{
		DSE_WATCH();
		if(!world.chunks.isChunkActive(chunk_id)) return;

		Chunk& chunk = world.chunks.getChunk(chunk_id);
		const size_t component_count = chunk.point_component_slot_ids.size();
		for(size_t component_index = 0; component_index < component_count; component_index++)
		{
			const PointComponentSlotId slot_id = chunk.point_component_slot_ids[component_index];
			if(slot_id >= world.point_data.getPointComponentSlotCount())
			{
				danikk_engine::formatFatalError("Chunk references invalid point component slot %", slot_id);
			}
			if(!world.point_data.isPointComponentSlotActive(slot_id)) continue;

			PointComponentSlot& slot = world.point_data.getPointComponentSlot(slot_id);
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
	void ComponentSystem::tickWorldBodyComponents()
	{
		DSE_WATCH();
		const DynamicArray<WorldBodyId> world_body_ids(world.world_body_data.getUsedWorldBodyIds());
		DynamicArray<WorldBodyComponentSlotId> component_slot_ids;
		DynamicArray<ChunkId> chunk_ids;

		for(WorldBodyId body_id : world_body_ids)
		{
			if(!world.world_body_data.isWorldBodyActive(body_id)) continue;
			component_slot_ids.clear();
			chunk_ids.clear();
			{
				const WorldBody& body = world.world_body_data.getWorldBody(body_id);
				for(WorldBodyComponentSlotId slot_id : body.component_slot_ids)
					component_slot_ids.push(slot_id);
				for(ChunkId chunk_id : body.chunk_ids)
					chunk_ids.push(chunk_id);
			}

			for(WorldBodyComponentSlotId slot_id : component_slot_ids)
			{
				if(!world.world_body_data.isWorldBodyComponentSlotActive(slot_id)) continue;
				WorldBodyComponentSlot& slot = world.world_body_data.getWorldBodyComponentSlot(slot_id);
				const WorldBodyComponentType& type = getWorldBodyComponentType(slot.type_id);
				if(type.tick != nullptr) type.tick(slot, world);
			}
			for(ChunkId chunk_id : chunk_ids)
			{
				if(world.chunks.isChunkActive(chunk_id)) tickChunkComponents(chunk_id);
			}
		}
	}

	/** @copydoc ComponentSystem::renderWorldBodyComponents */
	void ComponentSystem::renderWorldBodyComponents()
	{
		DSE_WATCH();
		const DynamicArray<WorldBodyId> world_body_ids(world.world_body_data.getUsedWorldBodyIds());
		DynamicArray<WorldBodyComponentSlotId> component_slot_ids;
		for(WorldBodyId body_id : world_body_ids)
		{
			if(!world.world_body_data.isWorldBodyActive(body_id)) continue;
			component_slot_ids.clear();
			for(WorldBodyComponentSlotId slot_id : world.world_body_data.getWorldBody(body_id).component_slot_ids)
				component_slot_ids.push(slot_id);

			for(WorldBodyComponentSlotId slot_id : component_slot_ids)
			{
				if(!world.world_body_data.isWorldBodyComponentSlotActive(slot_id)) continue;
				WorldBodyComponentSlot& slot = world.world_body_data.getWorldBodyComponentSlot(slot_id);
				const WorldBodyComponentType& type = getWorldBodyComponentType(slot.type_id);
				if(type.frame != nullptr) type.frame(slot, world);
			}
		}
	}

	/** @copydoc ComponentSystem::renderChunkComponents */
	void ComponentSystem::renderChunkComponents()
	{
		DSE_WATCH();
		const DynamicArray<WorldBodyId> world_body_ids(world.world_body_data.getUsedWorldBodyIds());
		DynamicArray<ChunkId> chunk_ids;
		for(WorldBodyId body_id : world_body_ids)
		{
			if(!world.world_body_data.isWorldBodyActive(body_id)) continue;
			chunk_ids.clear();
			for(ChunkId chunk_id : world.world_body_data.getWorldBody(body_id).chunk_ids)
				chunk_ids.push(chunk_id);

			for(ChunkId chunk_id : chunk_ids)
			{
				if(!world.chunks.isChunkActive(chunk_id)) continue;
				const Chunk& chunk = world.chunks.getChunk(chunk_id);
				const size_t component_count = chunk.point_component_slot_ids.size();
				for(size_t component_index = 0; component_index < component_count; component_index++)
				{
					const PointComponentSlotId slot_id = chunk.point_component_slot_ids[component_index];
					if(slot_id >= world.point_data.getPointComponentSlotCount())
					{
						danikk_engine::formatFatalError("Chunk references invalid point component slot %", slot_id);
					}
					if(!world.point_data.isPointComponentSlotActive(slot_id)) continue;

					PointComponentSlot& slot = world.point_data.getPointComponentSlot(slot_id);
					if(slot.uses_external_data && slot.external_data == nullptr)
					{
						danikk_engine::formatFatalError("Point component slot % has no external data", slot_id);
					}
					const PointComponentType& type = getPointComponentType(slot.type_id);
					if(type.frame != nullptr) type.frame(slot, world);
				}
			}
		}
	}
}
