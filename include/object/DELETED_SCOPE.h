#pragma once

#include <object/block_map.h>
#include <block/data.h>

#include <danikk_framework/array.h>

namespace danikk_space_engine
{
	namespace global_scope
	{
		struct BlockContext
		{
			BlockSlot* current_block = NULL;
			BlockMapChunk* current_chunk = NULL;
			BlockMapRegion* current_region = NULL;
			BlockMapObject* current_map = NULL;

			pos_type current_chunk_pos;
			pos_type current_block_pos;

			pos_type getGlobalBlockOffset()
			{
				return (current_region->pos * (int)BlockMapRegion::axis_size + current_chunk_pos) * (int)BlockMapChunk::axis_size;
			}
		};

		extern thread_local Array<BlockContext, 4> context_array;

		class GlobalBlockScope
		{
			bool32 has_prev = false;
		public:
			GlobalBlockScope() = default;

			GlobalBlockScope(BlockMapChunk& value, pos_type pos)
			{
				if(context_array.last().current_chunk != &value)
				{
					has_prev = true;
					context_array.duplicateLast();
				}
				BlockContext& last = context_array.last();
				last.current_block = &value[pos];
				last.current_block_pos = pos;
			}

			~GlobalBlockScope()
			{
				if(has_prev)
				{
					context_array.pop();
				}
				else
				{
					context_array.last().current_block = NULL;
				}
			}
		};

		class GlobalChunkScope
		{
			bool32 has_prev = false;
		public:
			GlobalChunkScope() = default;

			GlobalChunkScope(BlockMapRegion& value, pos_type pos)
			{
				if(context_array.last().current_chunk != &value[pos])
				{
					has_prev = true;
					context_array.duplicateLast();
				}
				BlockContext& last = context_array.last();
				last.current_chunk = &value[pos];
				last.current_chunk_pos = pos;
			}

			~GlobalChunkScope()
			{
				if(has_prev)
				{
					context_array.pop();
				}
				else
				{
					context_array.last().current_chunk = NULL;
				}
			}
		};

		class GlobalRegionScope
		{
			bool32 has_prev = false;
		public:
			GlobalRegionScope() = default;

			GlobalRegionScope(BlockMapRegion& value)
			{

				if(context_array.last().current_region != &value)
				{
					has_prev = true;
					context_array.duplicateLast();
				}
				BlockContext& last = context_array.last();
				last.current_region = &value;
			}

			~GlobalRegionScope()
			{
				if(has_prev)
				{
					context_array.pop();
				}
				else
				{
					context_array.last().current_region = NULL;
				}
			}
		};

		class GlobalMapScope
		{
		public:
			GlobalMapScope() = default;

			GlobalMapScope(BlockMapObject& value)
			{
				context_array.pushCtor();
				BlockContext& last = context_array.last();
				last.current_map = &value;
			}

			~GlobalMapScope()
			{
				context_array.pop();
			}
		};
	}

	using global_scope::BlockContext;

	BlockContext& getCurrentContext();
}
