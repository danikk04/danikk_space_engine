#include <danikk_space_engine/voxel/voxel.h>

#include <cstdlib>

namespace danikk_space_engine
{
	VoxelChunk::VoxelChunk(ivec3 coordinates, uint32 resolution)
		: coordinates(coordinates), resolution(resolution)
	{
		voxels.resize(static_cast<size_t>(resolution) * resolution * resolution, Voxel{});
	}

	VoxelMap::VoxelMap(float voxel_size, uint32 chunk_resolution)
		: voxel_size(voxel_size), chunk_resolution(chunk_resolution)
	{
		if(voxel_size <= 0.0f || chunk_resolution == 0)
		{
			std::abort();
		}
		chunks.reserve(16);
	}

	VoxelChunk* VoxelMap::findChunk(ivec3 coordinates)
	{
		for(VoxelChunk& chunk : chunks)
		{
			if(chunk.coordinates == coordinates)
			{
				return &chunk;
			}
		}
		return nullptr;
	}

	const VoxelChunk* VoxelMap::findChunk(ivec3 coordinates) const
	{
		for(const VoxelChunk& chunk : chunks)
		{
			if(chunk.coordinates == coordinates)
			{
				return &chunk;
			}
		}
		return nullptr;
	}

	VoxelChunk& VoxelMap::createChunk(ivec3 coordinates)
	{
		if(VoxelChunk* existing = findChunk(coordinates))
		{
			return *existing;
		}
		if(chunks.size() >= chunks.capacity()) std::abort();
		return chunks.pushCtor(coordinates, chunk_resolution);
	}
}
