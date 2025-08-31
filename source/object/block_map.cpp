#include <default.h>

#include <object/block_map.h>
#include <danikk_engine/dynamic_mesh.h>
#include <danikk_framework/glm.h>
#include <block/block.h>
#include <block/data.h>
#include <material.h>

namespace danikk_space_engine
{
	thread_local BlockMapChunk* current_chunk;
	thread_local BlockMapRegion* current_region;
	thread_local BlockMapObject* current_map;

	thread_local pos_type current_chunk_pos;
	thread_local pos_type current_block_pos;

	thread_local BlockSlot* current_block;

	Block* BlockBaseHeader::getBlockType()
	{
		return block_table[id];
	}

	vec3 getBLockOffset()
	{
		return vec3(current_region->pos) * (float)BlockMapRegion::block_axis_size + vec3(current_chunk_pos) * (float)BlockMapChunk::axis_size;
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

		for(const pos_type& pos : current_chunk->iteratePos())
		{
			uint32 block_id = (*current_chunk)[pos].getId();
			if(block_id == 0 || block_id != this->block_id)
			{
				continue;
			}
			for(const pos_type& direction : block_directions)
			{
				uvec3 directed_pos = pos_type(pos + direction);
				if(current_chunk->isValidIndex(directed_pos))
				{
					uint32 directed_block_id = (*current_chunk)[directed_pos].getId();
					if(directed_block_id != 0)
					{
						continue;
					}
				}

				vec3 offseted = vec3(pos) + vec3(0.5f) + vec3(direction) / 2.0f + getBLockOffset();

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

		for(BlockSlot& block : *current_chunk)
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
		logInfo(filledBlockCount());
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
			current_block_pos = pos;
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
		current_region = this;
		for(pos_type pos : data.iteratePos())
		{
			BlockMapChunk& element = data[pos];
			if(element.flags.is_active)
			{
				current_chunk_pos = pos;
				element.tick();
			}
		}
		current_region = NULL;
	}

	void BlockMapRegion::frame()
	{
		current_region = this;
		for(pos_type pos : data.iteratePos())
		{
			BlockMapChunk& element = data[pos];
			if(element.flags.is_exits)
			{
				element.frame();
			}
		}
		current_region = NULL;
	}

	void BlockMapRegion::regenerateMesh()
	{
		current_region = this;
		for(pos_type pos : data.iteratePos())
		{
			BlockMapChunk& chunk = data[pos];
			current_chunk_pos = pos;
			current_chunk = &chunk;
			if(chunk.flags.is_exits)
			{
				current_chunk = &chunk;
				chunk.regenerateMesh();
			}
		}
		current_region = NULL;
	}

	void BlockMapRegion::checkExits()
	{
		current_region = this;
		flags.is_exits = false;
		for(BlockMapChunk& chunk : *this)
		{
			chunk.checkExits();
			if(chunk.flags.is_exits)
			{
				flags.is_exits = true;
			}
		}
		current_region = NULL;
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

	RegionAllocator& BlockMapRegion::getAllocator()
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

	pos_type BlockMapRegion::regionPosToChunkIndex(pos_type pos)
	{
		return pos_type(pos.x / BlockMapChunk::size.x, pos.y / BlockMapChunk::size.y, pos.z / BlockMapChunk::size.z);
	}

	pos_type BlockMapRegion::regionPosToChunkPos(pos_type pos)
	{
		return pos_type(pos.x % BlockMapChunk::size.x, pos.y % BlockMapChunk::size.y, pos.z % BlockMapChunk::size.z);
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

	BlockSlot* BlockMapObject::getBlock(const pos_type& pos)
	{
		pos_type in_region_pos = globalPosToRegionPos(pos);
		pos_type region_index = globalPosToRegionIndex(pos);
		pos_type chunk_index = BlockMapRegion::regionPosToChunkIndex(in_region_pos);
		pos_type in_chunk_pos = BlockMapRegion::regionPosToChunkPos(in_region_pos);
		BlockMapRegion* region = get(region_index);
		current_region = region;
		if(region == NULL)
		{
			return NULL;
		}
		else
		{
			return &(*region)[chunk_index][in_chunk_pos];
		}
	}

	pos_type BlockMapObject::globalPosToRegionIndex(pos_type pos)
	{
		return pos_type(pos.x / BlockMapRegion::block_axis_size, pos.y / BlockMapRegion::block_axis_size, pos.z / BlockMapRegion::block_axis_size);
	}

	pos_type BlockMapObject::globalPosToRegionPos(pos_type pos)
	{
		if(pos.x < 0) pos.x--;
		if(pos.y < 0) pos.y--;
		if(pos.z < 0) pos.z--;
		pos_type result = pos_type(
				mod(pos.x, (int)BlockMapRegion::block_axis_size),
				mod(pos.y, (int)BlockMapRegion::block_axis_size),
				mod(pos.z, (int)BlockMapRegion::block_axis_size));
		return result;
	}

	void BlockMapObject::tick()
	{
		PhysicObject::tick();
		current_map = this;
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
		Object::frame();
		current_map = this;
		setWorldMatrix(mat4(1.0f));
		setDrawColor(vec4(1.0f));

		for(BlockMapRegion& element : data)
		{
			element.frame();
		}
	}

	void BlockMapObject::regenerateMesh()
	{
		for(BlockMapRegion& element : data)
		{
			if(element.flags.is_exits)
			{
				current_region = &element;
				element.regenerateMesh();
			}
		}
	}

	void BlockMapObject::checkExits()
	{
		for(BlockMapRegion& element : data)
		{
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
