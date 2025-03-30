#pragma once

#include <default.h>
#include <object/physic.h>
#include <danikk_engine/mesh.h>
#include <danikk_framework/tensor.h>
#include <danikk_framework/dictionary.h>
#include <block/block.h>

namespace game
{
	struct Block
	{
		uint32 id = 0;
	};

	class BlockMapChunk
	{
		static constexpr size_t axis_size = 16;
		static constexpr uvec3 size = uvec3(axis_size);
		FixedTensor<Block, size> data;
		Mesh mesh;
	public:
		void regenerateMesh();

		void tick();

		void frame();
	};

	class BlockMapRegion
	{
		static constexpr size_t axis_size = 16;
		static constexpr uvec3 size = uvec3(axis_size);
		FixedTensor<BlockMapChunk, size> data;
	public:
		void tick();

		void frame();

	};

	class RegionContainer//нужно будет передалать мd в древо
	{
		struct data_t
		{
			uvec3 key;
			BlockMapRegion region;
		};
		DynamicArray<data_t> data;

		void tick();

		void frame();
	public:
		BlockMapRegion& operator[](const uvec3& key);
	};

	class BlockMapObject : public virtual PhysicObject//может быть как и судном, так и землёй
	{
	public:
		RegionContainer data;

		void tick() override;

		void frame() override;

	};
}
