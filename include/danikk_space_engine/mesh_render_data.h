#pragma once

#include <danikk_framework/dynamic_array.h>
#include <danikk_engine/mesh.h>
#include <danikk_space_engine/material.h>

namespace danikk_space_engine
{
	/** @brief Outcome of attempting to draw a mesh record. */
	enum class MeshDrawResult : uint8
	{
		missing_mesh,
		empty_mesh,
		drawn
	};

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

	/** @brief Converts indexed triangles into a flat-shaded GPU mesh grouped by material. */
	void createMeshRenderData(MeshRenderData& record, const vec3* positions, size_t position_count,
		const uint32* triangle_indices, size_t index_count,
		const MaterialId* triangle_material_ids, size_t material_count);
	/** @brief Draws material ranges using a caller-provided camera-relative transform. */
	MeshDrawResult drawMeshRenderData(const MeshRenderData& record, const mat4& world_matrix);
	/** @brief Releases a mesh record's GPU resources and material ranges. */
	void destroyMeshRenderData(MeshRenderData& record);
}
