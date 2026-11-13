#pragma once

#include <danikk_framework/dynamic_array.h>

#include <danikk_space_engine/world/chunk.h>

namespace danikk_space_engine
{
	using WorldBodyId = uint32;

	class WorldBody
	{
	public:
		vec3 position = vec3(0.0f);
		DynamicArray<ChunkId> chunk_ids;

		void addChunk(ChunkId chunk_id);
		bool removeChunk(ChunkId chunk_id);
	};
}
