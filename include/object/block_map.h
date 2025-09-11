#pragma once

#include <default.h>
#include <object/physic.h>
#include <block/block.h>
#include <block/context.h>
#include <danikk_engine/mesh.h>
#include <danikk_framework/tensor.h>
#include <danikk_framework/dictionary.h>
#include <block/block.h>
#include <block/data.h>
#include <block/allocator.h>

namespace danikk_space_engine
{
	struct PosIndexPair
	{
		pos_type index;
		pos_type pos;
	};

	inline const pos_type block_directions[6]
	{
		vec3(1, 0, 0),
		vec3(0, 1, 0),
		vec3(0, 0, 1),

		vec3(-1, 0, 0),
		vec3(0, -1, 0),
		vec3(0, 0, -1),
	};

	pos_type getBlockOffset();

	struct block_collection_flags//флаги как для чанков, так и для регионов
	{
		bool32 is_exits = false;//есть ли хотя-бы один не вакуумный блок в этом чанке
		bool16 is_active = false;//есть ли хотя-бы один работающие механизм в чанке или идёт ли теплообмен между блоками.
		bool16 is_mesh_changed = false;
	};

	struct BlockMeshGroup
	{
		Mesh mesh;
		Texture texture;
		uint32 block_id = -1;
		uint32 material_id = -1;

		BlockMeshGroup() = default;

		~BlockMeshGroup()
		{
			if(!mesh.isNull())
			{
				mesh.free();
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

		BlockSlot& operator[](const pos_type& pos);

		BlockContext findGet(const pos_type& pos);

		BlockSlot* begin();

		BlockSlot* end();

		void checkExits();

		inline bool isValidIndex(const pos_type& index) { return data.isValidIndex(index); };

		size_t filledBlockCount();

		TensorIterable<BlockMapChunk::size> iteratePos();
	};

	class BlockMapRegion
	{
	public:
		static constexpr size_t axis_size = 2;
		static constexpr pos_type size = pos_type(axis_size);

		static constexpr size_t block_axis_size = BlockMapChunk::axis_size * axis_size;
		static constexpr pos_type block_size = pos_type(block_axis_size);
		//static constexpr size_t block_size = BlockMapChunk::size * size;
	private:
		MonolithAllocator allocator;
		FixedTensor<BlockMapChunk, size> data;
	public:
		//ЗАМЕТКА для нормальной поддержки отрицательных координат pos_type должен поддерживать отрицательный ноль
		pos_type pos;
		block_collection_flags flags;

		void tick();

		void frame();

		BlockMapChunk& operator[](const pos_type&);

		BlockMapChunk* begin();

		BlockMapChunk* end();

		void regenerateMesh();

		void checkExits();

		inline bool isValidIndex(const pos_type& index) { return data.isValidIndex(index); };

		int32 randCoord();

		pos_type randPos();

		uint filledBlockCount();

		MonolithAllocator& getAllocator();
	};

	class BlockMapObject : public virtual PhysicObject
	{
		DynamicArray<BlockMapRegion> data;
	public:
		void tick() override;

		void frame() override;

		void borderFrame();

		BlockMapRegion& operator[](const pos_type&);

		BlockMapRegion* get(const pos_type&);

		BlockContext getBlock(const pos_type&);

		void regenerateMesh();

		void checkExits();

		uint filledBlockCount();
	};
}
