#pragma once

#include <default.h>
#include <block/block.h>
#include <block/context.h>
#include <danikk_engine/mesh.h>
#include <danikk_framework/tensor.h>
#include <danikk_framework/dictionary.h>
#include <block/block.h>
#include <block/data.h>
#include <block/allocator.h>
#include <block/pos.h>
#include "kinetic.h"

namespace danikk_space_engine
{
	inline const ivec3 block_directions[6]
	{
		ivec3(1, 0, 0),
		ivec3(0, 1, 0),
		ivec3(0, 0, 1),

		ivec3(-1, 0, 0),
		ivec3(0, -1, 0),
		ivec3(0, 0, -1),
	};

	global_pos_type getBlockOffset();

	struct block_collection_flags//флаги как для чанков, так и для регионов
	{
		bool32 is_exits = false;//есть ли хотя-бы один не вакуумный блок в этом чанке
		bool16 is_active = false;//есть ли хотя-бы один работающие механизм в чанке или идёт ли теплообмен между блоками.
		bool16 is_changed = false;
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
		static constexpr size_t axis_size = 32;
		static constexpr ivec3 size = ivec3(axis_size);
	private:
		FixedTensor<BlockSlot, size> data;
		BlockMeshGroupCollection mesh_groups;
	public:
		block_collection_flags flags;

		BlockMapChunk() = default;

		void regenerateMesh();

		void tick();

		void frame();

		BlockSlot& operator[](const ivec3& block_pos);

		BlockContext findGet(const ivec3& block_pos);

		BlockSlot* begin();

		BlockSlot* end();

		void checkExits();

		inline bool isValidIndex(const ivec3& index) { return data.isValidIndex(index); };

		size_t filledBlockCount();

		TensorIterable<BlockMapChunk::size> iteratePos();
	};

	class BlockMapRegion
	{
	public:
		static constexpr size_t axis_size = 1;//УСТАНОВИЛ РАЗМЕР РЕГИОНА В 1, В ДАЛЬНЕЙШЕМ НУЖНО БУДЕТ ОТРЕФАКТОРИТЬ КОД ДЛЯ ПОДДЕРЖКИ ЗНАЧЕНИЙ ВЫШЕ 1, ЛИБО ПОЛНОСТЬЮ СЛИТЬ КЛАССЫ ЧАНКА И РЕГИОНА
		static constexpr ivec3 size = ivec3(axis_size);

		static constexpr size_t block_axis_size = BlockMapChunk::axis_size * axis_size;
		static constexpr ivec3 block_size = ivec3(block_axis_size);
		//static constexpr size_t block_size = BlockMapChunk::size * size;
	private:
		MonolithAllocator allocator;
		FixedTensor<BlockMapChunk, size> data;
	public:
		//ЗАМЕТКА для нормальной поддержки отрицательных координат pos_type должен поддерживать отрицательный ноль, в будущем нужно будет добавить поддержку. Возможно нужно в pos_type использовать float.
		ivec3 pos;
		block_collection_flags flags;

		void tick();

		void frame();

		BlockMapChunk& operator[](const ivec3& chunk_pos);

		BlockMapChunk* begin();

		BlockMapChunk* end();

		void regenerateMesh();

		void checkExits();

		inline bool isValidIndex(const ivec3& index) { return data.isValidIndex(index); };

		int32 randCoord();

		ivec3 randPos();

		uint filledBlockCount();

		MonolithAllocator& getAllocator();
	};

	namespace object_tags
	{
		class BlockMap
		{
			DynamicArray<BlockMapRegion> data;
		public:
			static uint32 id;

			void tick();

			void frame();

			void atDispose(){}

			void borderFrame();

			BlockMapRegion& operator[](const ivec3& region_pos);

			BlockMapRegion* get(const ivec3& region_pos);

			BlockContext get(const global_pos_type& global_pos);

			void destroyBlock();

			void regenerateMesh();

			void checkExits();

			uint filledBlockCount();
		};
	}
}
