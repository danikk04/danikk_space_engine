#include <default.h>

#include <object/block_map.h>
#include <block/context.h>

#include <danikk_engine/dynamic_mesh.h>
#include <danikk_framework/glm.h>
#include <block/block.h>
#include <block/data.h>
#include <block/pos.h>
#include <block/border.h>
#include <material.h>
#include <manager.h>

namespace danikk_space_engine
{
	Block* BlockBaseHeader::getBlockType()
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

		for(const pos_type& pos : current_block_context->chunk->iteratePos())
		{
			uint32 block_id = (*current_block_context->chunk)[pos].getId();
			if(block_id == 0 || block_id != this->block_id)
			{
				continue;
			}
			for(const pos_type& direction : block_directions)
			{
				pos_type directed_pos = pos_type(pos + direction);
				BlockContext block;
				pos_type block_global_offset = current_block_context->getGlobalBlockOffset();
				block = current_block_context->chunk->findGet(directed_pos);
				if(block.block == NULL || block.block->getId() != 0)
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
			if(!block.isHeaderExits())
			{
				continue;
			}
			bool block_group_exits = false;
			for(BlockMeshGroup& group : data)
			{
				if(group.block_id == block.getId())
				{
					block_group_exits = true;
					break;
				}
			}
			if(!block_group_exits)
			{
				BlockMeshGroup& group = data.pushCtor();
				BlockBaseHeader& header = block.getHeader();

				group.block_id = header.id;
				group.texture = getMaterialTexture(header.main_material_id);
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
		//logInfo(filledBlockCount());
	}

	void BlockMapChunk::tick()
	{
		for(pos_type pos : data.iteratePos())
		{
			BlockSlot& block = data[pos];
			if(!block.isHeaderExits())
			{
				continue;
			}
			BlockBaseHeader& header = block.getHeader();
			current_block_context->block = &(*this)[pos];
			header.getBlockType()->tick();
		}
	}

	void BlockMapChunk::frame()
	{
		if(flags.is_mesh_changed)
		{
			regenerateMesh();
			flags.is_mesh_changed = false;
		}
		mesh_groups.frame();
	}

	void BlockMapChunk::checkExits()
	{
		for(BlockSlot& block : *this)
		{
			if(block.getId() != 0)
			{
				flags.is_exits = true;
				return;
			}
		}
		flags.is_exits = false;
	}

	size_t BlockMapChunk::filledBlockCount()
	{
		if(!flags.is_exits)
		{
			return 0;
		}
		uint result = 0;
		for(BlockSlot& block : *this)
		{
			if(block.getId() != 0)
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

	BlockSlot& BlockMapChunk::operator[](const pos_type& pos)
	{
		return data[pos];
	}

	BlockContext BlockMapChunk::findGet(const pos_type& pos)
	{
		BlockContext context = *current_context_stack.last();
		BlockContextUser user(&context);

		if(pos.x < 0 || pos.y < 0 || pos.z < 0)
		{
			context.block = NULL;
			return context;
		}

		//В рамках чанка
		if(isValidIndex(pos))
		{
			context.block = &data[pos];
			return context;
		}

		//в рамках региона
		context.chunk_pos += PosConvetrer::blockPosToChunkOffset(pos);

		if(current_block_context->region->isValidIndex(context.chunk_pos))
		{
			pos_type validated_block_pos = PosConvetrer::validateBlockPos(pos);
			context.chunk = &(*context.region)[context.chunk_pos];
			context.block = &(*context.chunk)[validated_block_pos];
			return context;
		}

		//в рамках карты
		pos_type region_pos = current_block_context->region->pos + PosConvetrer::chunkPosToRegionOffset(context.chunk_pos);
		context.region = current_block_context->map->get(region_pos);
		if(context.region != NULL)
		{
			pos_type validated_block_pos = PosConvetrer::validateBlockPos(pos);
			pos_type validated_chunk_pos = PosConvetrer::validateChunkPos(context.chunk_pos);
			context.chunk = &(*context.region)[validated_chunk_pos];
			context.block = &(*context.chunk)[validated_block_pos];
			return context;
		}
		context.block = NULL;
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
		for(pos_type pos : data.iteratePos())
		{
			BlockMapChunk& element = data[pos];
			if(element.flags.is_active)
			{
				current_block_context->chunk = &element;
				current_block_context->block_pos = pos;
				element.tick();
			}
		}
	}

	void BlockMapRegion::frame()
	{
		current_block_context->region = this;
		for(pos_type pos : data.iteratePos())
		{
			BlockMapChunk& element = data[pos];
			if(element.flags.is_exits)
			{
				current_block_context->chunk = &element;
				current_block_context->chunk_pos = pos;
				element.frame();
			}
		}
	}

	void BlockMapRegion::regenerateMesh()
	{
		current_block_context->region = this;
		for(pos_type pos : data.iteratePos())
		{
			BlockMapChunk& element = data[pos];
			if(element.flags.is_exits)
			{
				current_block_context->chunk = &element;
				current_block_context->chunk_pos = pos;
				element.regenerateMesh();
			}
		}
	}

	void BlockMapRegion::checkExits()
	{
		current_block_context->region = this;
		flags.is_exits = false;
		for(pos_type pos : data.iteratePos())
		{
			BlockMapChunk& element = data[pos];
			element.checkExits();
			if(element.flags.is_exits)
			{
				current_block_context->chunk = &element;
				current_block_context->block_pos = pos;
			}
		}
	}

	int32 BlockMapRegion::randCoord()
	{
		return default_random.number<uint32>(0, BlockMapRegion::block_axis_size);
	}

	pos_type BlockMapRegion::randPos()
	{
		return pos_type(randCoord(), randCoord(), randCoord());
	}

	uint BlockMapRegion::filledBlockCount()
	{
		if(!flags.is_exits)
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

	BlockMapChunk& BlockMapRegion::operator[](const pos_type& pos)
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

	BlockMapRegion& BlockMapObject::operator[](const pos_type& pos)
	{
		for(BlockMapRegion& element : data)
		{
			if(element.pos == pos)
			{
				return element;
			}
		}
		BlockMapRegion& new_region = data.pushCtor();
		new_region.pos = pos;
		return new_region;
	}

	BlockMapRegion* BlockMapObject::get(const pos_type& pos)
	{
		for(BlockMapRegion& element : data)
		{
			if(element.pos == pos)
			{
				return &element;
			}
		}
		return NULL;
	}

	BlockContext BlockMapObject::getBlock(const pos_type& pos)
	{
		pos_type in_region_pos = PosConvetrer::globalPosToInChunkPos(pos);
		pos_type region_index = PosConvetrer::globalPosToRegionPos(pos);
		BlockContext result;
		BlockContext* saved_context = current_block_context;
		current_block_context = &result;
		result.map = this;
		result.region = get(region_index);
		if(result.region == NULL)
		{
			return result;
		}
		else
		{
			pos_type chunk_index = PosConvetrer::regionPosToChunkPos(in_region_pos);
			pos_type in_chunk_pos = PosConvetrer::regionPosToBlockPos(in_region_pos);
			result.chunk = &(*result.region)[chunk_index];
			result.block = &(*result.chunk)[in_chunk_pos];

			result.chunk_pos = chunk_index;
			result.block_pos = in_chunk_pos;
			current_block_context = saved_context;
			return result;
		}
	}

	void BlockMapObject::tick()
	{
		BlockContext context;
		current_block_context = &context;
		current_block_context->map = this;
		PhysicObject::tick();
		for(BlockMapRegion& element : data)
		{
			if(element.flags.is_active)
			{
				element.tick();
			}
		}
	}

	void BlockMapObject::frame()
	{
		BlockContext context;
		current_block_context = &context;
		current_block_context->map = this;

		setWorldMatrix(mat4(1.0f));
		setDrawColor(vec4(1.0f));

		for(BlockMapRegion& element : data)
		{
			element.frame();
		}

		Object::frame();
	}

	void BlockMapObject::borderFrame()
	{
		/*vec3 camera_pos = game_manager.main_camera->pos;
		pos_type global_pos = worldPosToGlobalPos(camera_pos);
		pos_type region_pos = globalPosToRegionPos;*/
	}


	void BlockMapObject::regenerateMesh()
	{
		BlockContext context;
		current_block_context = &context;
		current_block_context->map = this;

		for(BlockMapRegion& element : data)
		{
			if(element.flags.is_exits)
			{
				current_block_context->region = &element;
				element.regenerateMesh();
			}
		}
	}

	void BlockMapObject::checkExits()
	{
		BlockContext context;
		current_block_context = &context;
		current_block_context->map = this;

		for(BlockMapRegion& element : data)
		{
			current_block_context->region = &element;
			element.checkExits();
		}
	}

	uint BlockMapObject::filledBlockCount()
	{
		uint result = 0;
		for(BlockMapRegion& element : data)
		{
			result += element.filledBlockCount();
		}
		return result;
	}
}
