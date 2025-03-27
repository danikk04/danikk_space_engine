#pragma once

#include <default.h>
#include <object/physic.h>
#include <danikk_engine/mesh.h>
#include <danikk_framework/tensor.h>
#include <danikk_framework/dictionary.h>
#include <block/type.h>

namespace game
{
	struct Block
	{
		uint32 type = 0;
	};

	class BlockMapChuck
	{
		static constexpr size_t axis_size = 16;
		static constexpr uvec3 size = uvec3(axis_size);
		FixedTensor<Block, size> data;
		Mesh mesh;

		void regenerateMesh();
	};

	class BlockMapRegion
	{
		static constexpr size_t axis_size = 16;
		static constexpr uvec3 size = uvec3(axis_size);
		FixedTensor<BlockMapChuck, size> data;
		Mesh mesh;

	};

	class RegionContainer//нужно будет передалать мd в древо на подобии map
	{
		struct data_t
		{
			uvec3 key;
			BlockMapRegion region;
		};
		DynamicArray<data_t> data;
	public:
	};

	class BlockMapObject : public virtual PhysicObject//может быть как и судном, так и землёй
	{
	public:
		RegionContainer data;

		void tick() override;

		void frame() override;

	};
}
