#pragma once

#include <danikk_space_engine/default.h>

#include <cstddef>

namespace danikk_space_engine
{
	using VoxelMaterialId = uint32;
	using VoxelTypeId = uint32;

	struct Voxel
	{
		VoxelTypeId type_id = 0;
		VoxelMaterialId material_id = 0;
		float mass = 0.0f;
		float temperature = 0.0f;
	};

	struct VoxelChunk
	{
		ivec3 coordinates;
		uint32 resolution;
		DynamicArray<Voxel> voxels;

		VoxelChunk(ivec3 coordinates, uint32 resolution);
	};

	struct VoxelMap
	{
		float voxel_size;
		uint32 chunk_resolution;
		DynamicArray<VoxelChunk> chunks;

		VoxelMap(float voxel_size, uint32 chunk_resolution);

		VoxelChunk* findChunk(ivec3 coordinates);
		const VoxelChunk* findChunk(ivec3 coordinates) const;
		VoxelChunk& createChunk(ivec3 coordinates);
	};

	// Type definitions keep special behavior out of each voxel's memory footprint.
	// A null callback means that this type has no per-tick behavior.
	struct VoxelTypeDefinition
	{
		using update_function_t = void (*)(Voxel& voxel, float delta_time);

		VoxelTypeId id;
		update_function_t update;
	};
}
