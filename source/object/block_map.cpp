#include <object/block_map.h>
#include <danikk_engine/dynamic_mesh.h>
#include <danikk_framework/glm.h>
#include <block/block.h>

namespace danikk_space_engine
{
	block::Block* BlockData::getBlockType()
	{
		return block_table[id];
	}

	const vec3 block_directions[6]
	{
		vec3(1, 0, 0),
		vec3(0, 1, 0),
		vec3(0, 0, 1),

		vec3(-1, 0, 0),
		vec3(0, -1, 0),
		vec3(0, 0, -1),
	};

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

	void BlockMeshGroup::regenerateMesh(BlockMapChunk& chunk)
	{
		DynamicMesh dynamic_mesh;

		for(const uvec3& pos : chunk.iteratePos())
		{
			uint32 block_id = chunk[pos].id;
			if(block_id == 0 || block_id != this->block_id)
			{
				continue;
			}
			for(const vec3& direction : block_directions)
			{
				vec3 offseted = vec3(pos) + vec3(0.5f) + direction / 2.0f;

				dynamic_mesh.addSquare(offseted, direction);
			}
		}
		mesh = dynamic_mesh.toMesh();
	}

	void BlockMeshGroup::frame()
	{
		texture.bind();
		mesh.draw();
	}

	void BlockMeshGroupCollection::regenerateMesh(BlockMapChunk& chunk)
	{
		data.clear();

		for(BlockData& block : chunk)
		{
			bool block_group_exits = false;
			for(BlockMeshGroup& group : data)
			{
				if(group.block_id == block.id)
				{
					block_group_exits = true;
				}
			}
			if(!block_group_exits)
			{
				data.pushCtor();
				data.last().block_id = block.id;
				data.last().regenerateMesh(chunk);
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
		mesh_groups.regenerateMesh(*this);
	}

	void BlockMapChunk::tick()
	{
		for(uvec3 pos : data.iteratePos())
		{
			data[pos].getBlockType()->tick();
		}
	}

	void BlockMapChunk::frame()
	{
		mesh_groups.frame();
	}

	void BlockMapChunk::checkExits()
	{
		for(BlockData& block : *this)
		{
			if(block.id != 0)
			{
				flags.is_exits = true;
				return;
			}
		}
		flags.is_exits = false;
	}

	uint BlockMapChunk::filledBlockCount()
	{
		if(!flags.is_exits)
		{
			return 0;
		}
		uint result = 0;
		for(BlockData& block : *this)
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

	BlockData& BlockMapChunk::operator[](const uvec3& pos)
	{
		return data[pos];
	}

	BlockData* BlockMapChunk::begin()
	{
		return data.begin();
	}

	BlockData* BlockMapChunk::end()
	{
		return data.end();
	}

	void BlockMapRegion::tick()
	{
		for(uvec3 pos : data.iteratePos())
		{
			BlockMapChunk& element = data[pos];
			if(element.flags.is_exits)
			{
				element.tick();
			}
		}
	}

	void BlockMapRegion::frame()
	{
		for(uvec3 pos : data.iteratePos())
		{
			BlockMapChunk& element = data[pos];
			if(element.flags.is_exits)
			{
				element.frame();
			}
		}
	}

	void BlockMapRegion::regenerateMesh()
	{
		for(uvec3 pos : data.iteratePos())
		{
			BlockMapChunk& element = data[pos];
			if(element.flags.is_exits)
			{
				data[pos].regenerateMesh();
			}
		}
	}

	void BlockMapRegion::checkExits()
	{
		flags.is_exits = false;
		for(BlockMapChunk& chunk : *this)
		{
			chunk.checkExits();
			if(chunk.flags.is_exits)
			{
				flags.is_exits = true;
			}
		}
	}

	uint BlockMapRegion::filledBlockCount()
	{
		if(!flags.is_exits)
		{
			return 0;
		}
		uint result = 0;
		for(uvec3 pos : data.iteratePos())
		{
			result += data[pos].filledBlockCount();
		}
		return result;
	}

	BlockMapChunk& BlockMapRegion::operator[](const uvec3& pos)
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

	uvec3 BlockMapRegion::regionPosToChunkIndex(uvec3 pos)
	{
		return uvec3(pos.x / BlockMapChunk::size.x, pos.y / BlockMapChunk::size.y, pos.z / BlockMapChunk::size.z);
	}

	uvec3 BlockMapRegion::regionPosToChunkPos(uvec3 pos)
	{
		return uvec3(pos.x % BlockMapChunk::size.x, pos.y % BlockMapChunk::size.y, pos.z % BlockMapChunk::size.z);
	}

	BlockMapRegion& BlockMapObject::operator[](const ivec3& pos)
	{
		for(data_t& element : data)
		{
			if(element.pos == pos)
			{
				return element.region;
			}
		}
		data_t& new_region = data.pushCtor();
		new_region.pos = pos;
		return new_region.region;
	}

	BlockData& BlockMapObject::getBlock(const ivec3& pos)
	{
		uvec3 in_region_pos = globalPosToRegionPos(pos);
		uvec3 region_index = globalPosToRegionIndex(pos);
		uvec3 chunk_index = BlockMapRegion::regionPosToChunkIndex(in_region_pos);
		uvec3 in_chunk_pos = BlockMapRegion::regionPosToChunkPos(in_region_pos);
		return (*this)[region_index][chunk_index][in_chunk_pos];
	}

	uvec3 BlockMapObject::globalPosToRegionIndex(ivec3 pos)
	{
		return uvec3(pos.x / BlockMapRegion::size.x, pos.y / BlockMapRegion::size.y, pos.z / BlockMapRegion::size.z);
	}

	uvec3 BlockMapObject::globalPosToRegionPos(ivec3 pos)
	{
		return uvec3(pos.x % BlockMapRegion::size.x, pos.y % BlockMapRegion::size.y, pos.z % BlockMapRegion::size.z);
	}

	void BlockMapObject::tick()
	{
		for(data_t& element : data)
		{
			element.region.tick();
		}
	}

	void BlockMapObject::frame()
	{
		setWorldMatrix(mat4(1.0f));
		setDrawColor(vec4(1.0f));

		for(data_t& element : data)
		{
			element.region.frame();
		}

	}

	void BlockMapObject::regenerateMesh()
	{
		for(data_t& element : data)
		{
			if(element.region.flags.is_exits)
			{
				element.region.regenerateMesh();
			}
		}
	}

	void BlockMapObject::checkExits()
	{
		for(data_t& element : data)
		{
			element.region.checkExits();
		}
	}

	uint BlockMapObject::filledBlockCount()
	{
		uint result = 0;
		for(data_t& element : data)
		{
			result += element.region.filledBlockCount();
		}
		return result;
	}
}
