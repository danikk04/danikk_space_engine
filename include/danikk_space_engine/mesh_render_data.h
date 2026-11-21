#pragma once

#include <danikk_framework/dynamic_array.h>
#include <danikk_engine/mesh.h>
#include <danikk_space_engine/material.h>

namespace danikk_space_engine
{
	/** @brief Index range for one material in a combined GPU mesh. */
	struct MeshMaterialRange
	{
		MaterialId material_id = 0;
		uint32 first_index = 0;
		uint32 index_count = 0;
	};

	/** @brief GPU mesh and material draw ranges owned by a spatial object. */
	struct MeshRenderData
	{
		Mesh* mesh = nullptr;
		DynamicArray<MeshMaterialRange> material_ranges;
	};
}
