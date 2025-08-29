#pragma once

#include <default.h>
#include <object/physic.h>
#include <block/block.h>
#include <danikk_engine/mesh.h>
#include <danikk_framework/tensor.h>
#include <danikk_framework/dictionary.h>
#include <block/block.h>
#include <block/data.h>
#include <block/allocator.h>

namespace danikk_space_engine
{
	inline const pos_type block_directions[6]
	{
		vec3(1, 0, 0),
		vec3(0, 1, 0),
		vec3(0, 0, 1),

		vec3(-1, 0, 0),
		vec3(0, -1, 0),
		vec3(0, 0, -1),
	};
	vec3 getBlockOffset();

	struct block_collection_flags//флаги как для чанков, так и для регионов
	{
		bool32 is_exits = false;//есть ли хотя-бы один не вакуумный блок в этом чанке
		bool32 is_active = false;//есть ли хотя-бы один работающие механизм в чанке или идёт ли теплообмен между блоками.
	};

	struct BlockMeshGroup
	{
		Mesh mesh;
		Texture texture = Texture("container");
		uint32 block_id = -1;

		BlockMeshGroup() = default;

		~BlockMeshGroup()
		{
			if(!mesh.isNull())
			{
				mesh.clear();
			}
		}

		void regenerateMesh();

		void frame();
	};

	class BlockMeshGroupCollection
	{
		DynamicArray<BlockMeshGroup> data;//потом нужно будет переделать так, чтобы память для групп блоков хранилась регионе одним куском

		bool containsBlockId(uint32 id);
	public:
		BlockMeshGroupCollection() = default;

		void regenerateMesh();

		void frame();
	};

	class BlockMapChunk
	{
	public:
		static constexpr size_t axis_size = 16;
		static constexpr pos_type size = pos_type(axis_size);
	private:
		FixedTensor<BlockSlot, size> data;
		BlockMeshGroupCollection mesh_groups;
	public:
		block_collection_flags flags;

		BlockMapChunk() = default;

		void regenerateMesh();

		void tick();

		void frame();

		BlockSlot& operator[](const pos_type&);

		BlockSlot* begin();

		BlockSlot* end();

		void checkExits();

		inline bool isValidIndex(const pos_type& index) { return data.isValidIndex(index); };

		uint filledBlockCount();

		TensorIterable<BlockMapChunk::size> iteratePos();
	};

	class BlockMapRegion
	{
	public:
		static constexpr size_t axis_size = 2;
		static constexpr pos_type size = pos_type(axis_size);

		static constexpr size_t block_axis_size = BlockMapChunk::axis_size * axis_size;
		//static constexpr size_t block_size = BlockMapChunk::size * size;
	private:
		RegionAllocator allocator;
		FixedTensor<BlockMapChunk, size> data;
	public:
		pos_type pos;
		block_collection_flags flags;

		void tick();

		void frame();

		BlockMapChunk& operator[](const pos_type&);

		BlockMapChunk* begin();

		BlockMapChunk* end();

		static pos_type regionPosToChunkIndex(pos_type pos);

		static pos_type regionPosToChunkPos(pos_type pos);

		void regenerateMesh();

		void checkExits();

		inline bool isValidIndex(const pos_type& index) { return data.isValidIndex(index); };

		void setAsCurrent();

		uint filledBlockCount();

		RegionAllocator& getAllocator();
	};

	class BlockMapObject : public virtual PhysicObject
	{
		DynamicArray<BlockMapRegion> data;
	public:
		void tick() override;

		void frame() override;

		BlockMapRegion& operator[](const pos_type&);

		BlockMapRegion* get(const pos_type&);

		BlockSlot& getBlock(const pos_type&);

		static pos_type globalPosToRegionIndex(pos_type pos);

		static pos_type globalPosToRegionPos(pos_type pos);

		void regenerateMesh();

		void checkExits();

		uint filledBlockCount();
	};

	extern thread_local BlockMapChunk* current_chunk;
	extern thread_local BlockMapRegion* current_region;
	extern thread_local BlockMapObject* current_map;

	extern thread_local pos_type current_chunk_pos;
	extern thread_local pos_type current_block_pos;
}
