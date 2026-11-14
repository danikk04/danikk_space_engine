#pragma once

#include <danikk_framework/dynamic_array.h>

#include <danikk_space_engine/world/chunk.h>
#include <danikk_space_engine/component/world_body/world_body.h>

namespace danikk_space_engine
{
	class WorldBody
	{
	public:
		glm::dvec3 position = glm::dvec3(0.0);
		DynamicArray<ChunkId> chunk_ids;
		DynamicArray<WorldBodyComponentSlotId> component_slot_ids;

		void addChunk(ChunkId chunk_id);
		bool removeChunk(ChunkId chunk_id);
		void addComponentSlot(WorldBodyComponentSlotId slot_id);
		bool removeComponentSlot(WorldBodyComponentSlotId slot_id);
	};
}
