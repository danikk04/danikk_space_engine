#include <default.h>

#include <object/block_map.h>
#include <object/world.h>
#include <block/context.h>

#include <danikk_engine/dynamic_mesh.h>
#include <danikk_framework/glm.h>
#include <block/block.h>
#include <block/data.h>
#include <block/pos.h>
#include <block/material.h>
#include <block/border.h>
#include <material.h>
#include <manager.h>

namespace danikk_space_engine
{
	Block* BlockSlot::getBlockType()
	{
		return block_table[id];
	}

	bool BlockMeshGroupCollection::containsBlockId(uint32 id)
	{
		for(BlockMeshGroup& element : data)
		{
			if(element.block_id == id)
			{
				return true;
			}
		}
		return false;
	}

	void BlockMeshGroup::regenerateMesh()
	{
		DynamicMesh<DefaultVertex> dynamic_mesh;

		dynamic_mesh.vertexesReserve(2048);

		for(const ivec3& pos : current_block_context->chunk->iteratePos())
		{
			uint32 block_id = (*current_block_context->chunk)[pos].id;
			if(block_id == 0 || block_id != this->block_id)
			{
				continue;
			}
			for(const ivec3& direction : block_directions)
			{
				ivec3 directed_pos = ivec3(pos + direction);
				BlockContext block;
				ivec3 block_global_offset = current_block_context->pos.getGlobalPos();
				block = current_block_context->chunk->findGet(directed_pos);
				if(block.block == NULL || block.block->id != 0)
				{
					continue;
				}

				vec3 offseted = vec3(pos) + vec3(0.5f) + vec3(direction) / 2.0f + vec3(block_global_offset);

				dynamic_mesh.addSquare(offseted, vec3(direction));
			}
		}
		dynamic_mesh.setDataToMesh(mesh);
		DefaultVertex::setAttributes();
	}

	void BlockMeshGroup::frame()
	{
		texture.bind();
		mesh.draw();
	}

	void BlockMeshGroupCollection::regenerateMesh()
	{
		/*for(BlockMeshGroup& group : data)
		{
			group.mesh.free();
			group.texture.free();
		}*/
		data.clear();

		for(BlockSlot& block : *current_block_context->chunk)
		{
			if(block.id == 0)
			{
				continue;
			}
			bool block_group_exits = false;
			for(BlockMeshGroup& group : data)
			{
				if(group.block_id == block.id)
				{
					block_group_exits = true;
					break;
				}
			}
			if(!block_group_exits)
			{
				BlockMeshGroup& group = data.pushCtor();

				group.block_id = block.id;
				useObjectTag((&block), BlockMaterial, material);
				group.texture = getMaterialTexture(material_tag->main_material_id);
				group.regenerateMesh();
			}
		}
	}

	void BlockMeshGroupCollection::frame()
	{
		for(BlockMeshGroup& group : data)
		{
			group.frame();
		}
	}

	void BlockMapChunk::regenerateMesh()
	{
		mesh_groups.regenerateMesh();
	}

	void BlockMapChunk::tick()
	{
		for(ivec3 pos : data.iteratePos())
		{
			BlockSlot& block = data[pos];
			if(!block.id != 0)
			{
				continue;
			}
			current_block_context->block = &(*this)[pos];
			//block.tick();
		}
	}

	void BlockMapChunk::frame()
	{
		if(flags.isChanged())
		{
			regenerateMesh();
			flags.setActive();
		}
		mesh_groups.frame();
	}

	void BlockMapChunk::checkExits()
	{
		for(BlockSlot& block : *this)
		{
			if(block.id != 0)
			{
				flags.setExits();
				return;
			}
		}
		flags.setNone();
	}

	size_t BlockMapChunk::filledBlockCount()
	{
		if(flags.isNone())
		{
			return 0;
		}
		uint result = 0;
		for(BlockSlot& block : *this)
		{
			if(block.id != 0)
			{
				result++;
			}
		}
		return result;
	}

	TensorIterable<BlockMapChunk::size> BlockMapChunk::iteratePos()
	{
		return data.iteratePos();
	}

	BlockSlot& BlockMapChunk::operator[](const ivec3& block_pos)
	{
		return data[block_pos];
	}

	BlockContext BlockMapChunk::findGet(const ivec3& block_pos)
	{
		BlockContext context = *current_block_context;
		BlockContextUser user(&context);

		if(block_pos.x < 0 || block_pos.y < 0 || block_pos.z < 0)
		{
			context.block = NULL;
			return context;
		}

		/*//В рамках чанка
		if(isValidIndex(block_pos))
		{
			context.block = &data[block_pos];
			return context;
		}*/

		//в рамках мира
		context.pos.addGlobalPos(block_pos);
		context = context.map->get(context.pos);
		return context;
	}

	BlockSlot* BlockMapChunk::begin()
	{
		return data.begin();
	}

	BlockSlot* BlockMapChunk::end()
	{
		return data.end();
	}

	void BlockMapRegion::tick()
	{
		current_block_context->region = this;
		for(ivec3 pos : data.iteratePos())
		{
			BlockMapChunk& element = data[pos];
			if(element.flags.isActive())
			{
				current_block_context->chunk = &element;
				current_block_context->pos.setChunkPos(pos);
				element.tick();
			}
		}
	}

	void BlockMapRegion::frame()
	{
		current_block_context->region = this;
		for(ivec3 pos : data.iteratePos())
		{
			BlockMapChunk& element = data[pos];
			if(element.flags.isExits())
			{
				current_block_context->chunk = &element;
				current_block_context->pos.setChunkPos(pos);
				element.frame();
			}
		}
	}

	void BlockMapRegion::regenerateMesh()
	{
		current_block_context->region = this;
		for(ivec3 pos : data.iteratePos())
		{
			BlockMapChunk& element = data[pos];
			if(element.flags.isExits())
			{
				current_block_context->chunk = &element;
				current_block_context->pos.setChunkPos(pos);
				element.regenerateMesh();
			}
		}
	}

	void BlockMapRegion::checkExits()
	{
		current_block_context->region = this;
		flags.setNone();
		for(ivec3 pos : data.iteratePos())
		{
			BlockMapChunk& element = data[pos];
			element.checkExits();
			if(element.flags.isExits())
			{
				current_block_context->chunk = &element;
				current_block_context->pos.setChunkPos(pos);
			}
		}
	}

	int32 BlockMapRegion::randCoord()
	{
		return default_random.number<uint32>(0, BlockMapRegion::block_axis_size);
	}

	ivec3 BlockMapRegion::randPos()
	{
		return ivec3(randCoord(), randCoord(), randCoord());
	}

	uint BlockMapRegion::filledBlockCount()
	{
		if(flags.isNone())
		{
			return 0;
		}
		uint result = 0;
		for(BlockMapChunk& chunk : data)
		{
			result += chunk.filledBlockCount();
		}
		return result;
	}

	MonolithAllocator& BlockMapRegion::getAllocator()
	{
		return allocator;
	}

	BlockMapChunk& BlockMapRegion::operator[](const ivec3& pos)
	{
		return data[pos];
	}

	BlockMapChunk* BlockMapRegion::begin()
	{
		return data.begin();
	}

	BlockMapChunk* BlockMapRegion::end()
	{
		return data.end();
	}
	namespace object_tags
	{
		BlockMapRegion& BlockMap::operator[](const ivec3& region_pos)
		{
			for(BlockMapRegion& element : data)
			{
				if(element.pos == region_pos)
				{
					return element;
				}
			}
			BlockMapRegion& new_region = data.pushCtor();
			Object* current_object = object_stack.peek();
			new_region.pos = current_object->getTag<object_tags::World>()->pos;
			return new_region;
		}

		BlockMapRegion* BlockMap::get(const ivec3& region_pos)
		{
			for(BlockMapRegion& element : data)
			{
				if(element.pos == region_pos)
				{
					return &element;
				}
			}
			return NULL;
		}

		BlockContext BlockMap::get(const global_pos_type& global_pos)
		{
			BlockContext result;
			BlockContext* saved_context = current_block_context;
			current_block_context = &result;
			result.map = this;
			thread_local BlockMapRegion* region_cache = NULL;
			ivec3 region_pos = global_pos.getRegionPos();
			if(region_cache == NULL || region_cache->pos != region_pos)
			{
				result.region = get(region_pos);
			}
			else
			{
				result.region = region_cache;
			}
			if(result.region == NULL)
			{
				return result;
			}
			result.chunk = &(*result.region)[global_pos.getChunkPos()];
			result.block = &(*result.chunk)[global_pos.getBlockPos()];

			result.pos = global_pos;
			current_block_context = saved_context;
			return result;
		}

		void BlockMap::destroyBlock()
		{
			current_block_context->block->id = 0;
			BlockMapChunk* current_chunk = current_block_context->chunk;
			block_collection_flags* chunk_flags = &current_chunk->flags;
			chunk_flags->setActive();

			/*for(ivec3 ipos : TensorIterable<uvec3(2,2,2)>())
			{
				ivec3 pos = ipos - ivec3(1,1,1);
			}*/
		}

		void BlockMap::tick()
		{
			BlockContext context;
			current_block_context = &context;
			current_block_context->map = this;
			for(BlockMapRegion& element : data)
			{
				if(element.flags.isActive())
				{
					element.tick();
				}
			}
		}

		void BlockMap::frame()
		{
			BlockContext context;
			current_block_context = &context;
			current_block_context->map = this;

			setWorldMatrix(mat4(1.0f));
			setDrawColor(vec4(1.0f));

			for(BlockMapRegion& element : data)
			{
				current_block_context->region = &element;
				current_block_context->pos.setRegionPos(current_block_context->region->pos);
				element.frame();
			}
		}

		void BlockMap::borderFrame()
		{
			/*vec3 camera_pos = game_manager.main_camera->pos;
			pos_type global_pos = worldPosToGlobalPos(camera_pos);
			pos_type region_pos = globalPosToRegionPos;*/
		}


		void BlockMap::regenerateMesh()
		{
			BlockContext context;
			current_block_context = &context;
			current_block_context->map = this;

			for(BlockMapRegion& element : data)
			{
				if(element.flags.isExits())
				{
					current_block_context->region = &element;
					current_block_context->pos.setRegionPos(current_block_context->region->pos);
					element.regenerateMesh();
				}
			}
		}

		void BlockMap::checkExits()
		{
			BlockContext context;
			current_block_context = &context;
			current_block_context->map = this;

			for(BlockMapRegion& element : data)
			{
				current_block_context->region = &element;
				current_block_context->pos.setRegionPos(current_block_context->region->pos);
				element.checkExits();
			}
		}

		uint BlockMap::filledBlockCount()
		{
			uint result = 0;
			for(BlockMapRegion& element : data)
			{
				result += element.filledBlockCount();
			}
			return result;
		}
	}
}
