#pragma once

#include <default.h>
#include <objects/object.h>

namespace sotiria_project
{
	struct Voxel
	{
		uint32 material = 0;
	};

	class VoxelMapObject : WorldObject
	{
		static constexpr size_t axis_size = 64;
		static constexpr uvec3 size = uvec3(axis_size);
	public:
		danikk_engine::FixedVoxelMap<Voxel, size> data;
	};
}
