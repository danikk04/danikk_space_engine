#pragma once

#include <default.h>
#include <danikk_engine/voxel_map.h>

namespace sotiria_project
{
	struct Voxel
	{
		uint32 material;
	};

	class Chunk
	{
		static constexpr size_t axis_size = 64;
		static constexpr uvec3 size = uvec3(axis_size);
	public:
		//danikk_engine::FixedVoxelMap<Voxel, size> data;

	};

	class VoxelMapComponent : BaseComponent
	{
	public:
		static constexpr size_t size_axis = 8;
		static constexpr uvec3 size = uvec3(size_axis);
		DEFINE_COMPONENT_ID

		DEFINE_COMPONENT_INIT_FUNCTION
		{

		}

		DEFINE_COMPONENT_FRAME_FUNCTION
		{

		}

		DEFINE_COMPONENT_TICK_FUNCTION
		{

		}
	};
}
