#include <block/pos.h>
#include <object/block_map.h>

namespace danikk_space_engine
{
	struct pos_bit_data
	{
		int size;
		int offset;
		int bitmask;

		pos_bit_data(int size, int offset, int bitmask) : size(size), offset(offset), bitmask(bitmask){}

		void setData(ivec3& target, const ivec3& data)
		{
			ivec3 offseted_data = ivec3(data.x << offset, data.y << offset, data.z << offset);
			target = ivec3(target.x & !bitmask, target.x & !bitmask, target.z & !bitmask);//затираем нулями
			target = ivec3(target.x | offseted_data.x, target.y | offseted_data.y, target.z | offseted_data.z);
		}

		ivec3 getData(ivec3 value)
		{
			ivec3 result = ivec3(value.x & bitmask, value.y & bitmask, value.z & bitmask);
			result = ivec3(result.x >> offset, result.y >> offset, result.z >> offset);
			return result;
		}
	};

	pos_bit_data block_pos_bits			(4, 0, 			0b00000000000000000000000000001111);
	pos_bit_data chunk_pos_bits			(0, 4, 			0b00000000000000000000000000000000);
	pos_bit_data region_pos_bits		(28, 4,			0b11111111111111111111111111110000);
	pos_bit_data chunk_block_pos_bits	(4, 0,			0b00000000000000000000000000001111);

	//ПОСКОЛЬКУ РАЗМЕР РЕГИОНА УСТАНОВИЛ В 1, ТО НА ПОЗИЦИЮ ЧАНКА УДЕЛЯЕТСЯ 0 БИТ, ЗНАЧЕНИЕ НИЖЕ ДЛЯ РАЗМЕРА 2 на 2 на 2
	/*pos_bit_data block_pos_bits			(4, 0, 			0b00000000000000000000000000001111);
	pos_bit_data chunk_pos_bits			(1, 4, 			0b00000000000000000000000000010000);
	pos_bit_data region_pos_bits		(27, 5,			0b11111111111111111111111111100000);
	pos_bit_data chunk_block_pos_bits	(5, 0,			0b00000000000000000000000000011111);*/

	ivec3 global_pos_type::getGlobalPos() const
	{
		return data;
	}

	void global_pos_type::setGlobalPos(ivec3 value)
	{
		data = value;
	}

	void global_pos_type::addGlobalPos(ivec3 value)
	{
		data += value;
	}

	ivec3 global_pos_type::getRegionPos() const
	{
		return region_pos_bits.getData(data);
	}

	void global_pos_type::setRegionPos(ivec3 value)
	{
		region_pos_bits.setData(data, value);
	}

	ivec3 global_pos_type::getChunkPos() const
	{
		return chunk_pos_bits.getData(data);
	}

	void global_pos_type::setChunkPos(ivec3 value)
	{
		chunk_pos_bits.setData(data, value);
	}

	ivec3 global_pos_type::getBlockPos() const
	{
		return block_pos_bits.getData(data);
	}

	void global_pos_type::setBlockPos(ivec3 value)
	{
		block_pos_bits.setData(data, value);
	}

	ivec3 global_pos_type::getChunkBlockPos() const
	{
		return chunk_block_pos_bits.getData(data);
	}

	void global_pos_type::setChunkBlockPos(ivec3 value)
	{
		chunk_block_pos_bits.setData(data, value);
	}
}
