#include <object/block_map.h>
#include <danikk_engine/dynamic_mesh.h>
#include <danikk_framework/glm.h>
#include <block/block.h>

namespace game
{
	block::Block* BlockData::getBlockType()
	{
		return block_table[id];
	}

	const vec3 block_directions[6]
	{
		uvec3(1, 0, 0),
		uvec3(0, 1, 0),
		uvec3(0, 0, 1),

		uvec3(-1, 0, 0),
		uvec3(0, -1, 0),
		uvec3(0, 0, -1),
	};

	void BlockMapChunk::regenerateMesh()
	{
		block_groups.resize(0);

		index_t used_block_group_index = 0;

		DynamicMesh dynamic_mesh;

		for(uvec3 pos : data.iteratePos())
		{
			if(data[pos].id == 0)
			{
				continue;
			}
			for(const vec3& directory : block_directions)
			{
				uvec3 offseted = uvec3((vec3)pos + directory);
				if(	offseted.x == axis_size || offseted.y == axis_size || offseted.z == axis_size ||
					offseted.x == 0 || offseted.y == 0 || offseted.z == 0 ||
					data[offseted].id == 0)
				{
					dynamic_mesh.addSquare(vec3(pos) + directory / 2.0f, directory);
				}
			}
		}
		block_groups[0].mesh = dynamic_mesh.toMesh();
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
		block_groups[0].mesh.draw();
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

	BlockMapRegion& BlockMapObject::operator[](const uvec3& pos)
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

	BlockData& BlockMapObject::getBlock(const uvec3& pos)
	{
		uvec3 in_region_pos = globalPosToRegionPos(pos);
		uvec3 region_index = globalPosToRegionIndex(pos);
		uvec3 chunk_index = BlockMapRegion::regionPosToChunkIndex(in_region_pos);
		uvec3 in_chunk_pos = BlockMapRegion::regionPosToChunkPos(in_region_pos);
		return (*this)[region_index][chunk_index][in_chunk_pos];
	}

	uvec3 BlockMapObject::globalPosToRegionIndex(uvec3 pos)
	{
		return uvec3(pos.x / BlockMapRegion::size.x, pos.y / BlockMapRegion::size.y, pos.z / BlockMapRegion::size.z);
	}

	uvec3 BlockMapObject::globalPosToRegionPos(uvec3 pos)
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
