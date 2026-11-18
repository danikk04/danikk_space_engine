#include <danikk_space_engine/profiler.h>
#include <danikk_space_engine/world/world.h>
#include <danikk_space_engine/component_render_system.h>

namespace danikk_space_engine
{
	/** @copydoc renderWorldBodyComponents */
	void renderWorldBodyComponents(World& world)
	{
		DSE_WATCH();
		const DynamicArray<WorldBodyId> world_body_ids(world.data.getUsedWorldBodyIds());
		DynamicArray<WorldBodyComponentSlotId> component_slot_ids;
		for(WorldBodyId body_id : world_body_ids)
		{
			if(!world.data.isWorldBodyActive(body_id)) continue;
			component_slot_ids.clear();
			for(WorldBodyComponentSlotId slot_id : world.data.getWorldBody(body_id).component_slot_ids)
				component_slot_ids.push(slot_id);

			for(WorldBodyComponentSlotId slot_id : component_slot_ids)
			{
				if(!world.data.isWorldBodyComponentSlotActive(slot_id)) continue;
				WorldBodyComponentSlot& slot = world.data.getWorldBodyComponentSlot(slot_id);
				const WorldBodyComponentType& type = getWorldBodyComponentType(slot.type_id);
				if(type.frame != nullptr) type.frame(slot, world);
			}
		}
	}

	/** @copydoc renderChunkComponents */
	void renderChunkComponents(World& world)
	{
		DSE_WATCH();
		const DynamicArray<WorldBodyId> world_body_ids(world.data.getUsedWorldBodyIds());
		DynamicArray<ChunkId> chunk_ids;
		for(WorldBodyId body_id : world_body_ids)
		{
			if(!world.data.isWorldBodyActive(body_id)) continue;
			chunk_ids.clear();
			for(ChunkId chunk_id : world.data.getWorldBody(body_id).chunk_ids)
				chunk_ids.push(chunk_id);

			for(ChunkId chunk_id : chunk_ids)
			{
				if(!world.data.isChunkActive(chunk_id)) continue;
				const Chunk& chunk = world.data.getChunk(chunk_id);
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
