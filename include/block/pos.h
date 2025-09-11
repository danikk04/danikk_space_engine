#pragma once

#include <default.h>

namespace danikk_space_engine
{
	//pos - координаты в матрице мира
	//coord - координаты в глобальном пространстве

	//global - общие координаты

	class PosConvetrer
	{
	public:

		static pos_type globalPosToRegionPos(pos_type pos);
		static pos_type globalPosToInChunkPos(pos_type pos);

		static pos_type chunkPosToRegionOffset(pos_type pos);
		static pos_type validateChunkPos(pos_type block_pos);

		static pos_type regionPosToChunkPos(pos_type pos);
		static pos_type regionPosToBlockPos(pos_type pos);

		static pos_type blockPosToChunkOffset(pos_type block_pos);
		static pos_type validateBlockPos(pos_type block_pos);

		static pos_type worldPosToGlobalCoord(vec3 pos);
		static vec3 chunkPosToChunkCenterCoord(pos_type chunk_pos);
	};
}
