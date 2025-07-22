#pragma once

#include <default.h>
#include <object/physic.h>
#include <block/block.h>
#include <danikk_engine/mesh.h>
#include <danikk_framework/tensor.h>
#include <danikk_framework/dictionary.h>
#include <block/block.h>

namespace danikk_space_engine
{
	struct BlockData
	{
		uint32 id = 0;

		block::Block* getBlockType();
	};

	struct block_collection_flags//флаги как для чанков, так и для регионов
	{
		bool32 is_exits = false;//есть ли хотя-бы один не вакуумный блок в этом чанке
		bool32 is_active = false;//есть ли хотя-бы один работающие механизм в чанке или идёт ли теплообмен между блоками.
	};

	class BlockMapChunk
	{
	public:
		static constexpr size_t axis_size = 16;
		static constexpr uvec3 size = uvec3(axis_size);
	private:
		FixedTensor<BlockData, size> data;
		struct BlockGroup
		{
			Mesh mesh;
			Texture texture;

			BlockGroup() = default;

			~BlockGroup()
			{
				if(!mesh.isNull())
				{
					mesh.clear();
				}
			}
		};
		DynamicArray<BlockGroup> block_groups;//потом нужно будет переделать так, чтобы память для групп блоков хранилась регионе одним куском
	public:
		block_collection_flags flags;

		BlockMapChunk() = default;

		void regenerateMesh();

		void tick();

		void frame();

		BlockData& operator[](const uvec3&);

		BlockData* begin();

		BlockData* end();

		void checkExits();

		uint filledBlockCount();
	};

	class BlockMapRegion
	{
	public:
		static constexpr size_t axis_size = 2;
		static constexpr uvec3 size = uvec3(axis_size);
	private:
		FixedTensor<BlockMapChunk, size> data;
	public:
		block_collection_flags flags;

		void tick();

		void frame();

		BlockMapChunk& operator[](const uvec3&);

		BlockMapChunk* begin();

		BlockMapChunk* end();

		static uvec3 regionPosToChunkIndex(uvec3 pos);

		static uvec3 regionPosToChunkPos(uvec3 pos);

		void regenerateMesh();

		void checkExits();

		uint filledBlockCount();
	};

	class BlockMapObject : public virtual PhysicObject
	{
		struct data_t
		{
			uvec3 pos;
			BlockMapRegion region;
		};
		DynamicArray<data_t> data;
	public:
		void tick() override;

		void frame() override;

		BlockMapRegion& operator[](const uvec3&);

		BlockData& getBlock(const uvec3&);

		static uvec3 globalPosToRegionIndex(uvec3 pos);

		static uvec3 globalPosToRegionPos(uvec3 pos);

		void regenerateMesh();

		void checkExits();

		uint filledBlockCount();
	};
}
