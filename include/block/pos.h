#pragma once

#include <default.h>

namespace danikk_space_engine
{
	//pos - координаты в матрице мира
	//coord - координаты в глобальном пространстве

	class global_pos_type
	{
	private:
		ivec3 data;//global pos - глобальная позиция
	public:
		ivec3 getGlobalPos() const;
		void setGlobalPos(ivec3 value);
		void addGlobalPos(ivec3 value);

		ivec3 getRegionPos() const;
		void setRegionPos(ivec3 value);

		ivec3 getChunkPos() const;
		void setChunkPos(ivec3 value);

		ivec3 getBlockPos() const;
		void setBlockPos(ivec3 value);

		ivec3 getChunkBlockPos() const;
		void setChunkBlockPos(ivec3 value);

		vec3 getWorldPos() const;
		void setWorldPos(vec3 value);
	};
}
