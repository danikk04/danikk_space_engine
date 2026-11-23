#pragma once

#include <danikk_framework/dynamic_array.h>
#include <danikk_space_engine/id.h>
#include <danikk_space_engine/mesh_render_data.h>

namespace danikk_space_engine
{
	/** @brief Construction data attached one-to-one to a WorldBody. */
	struct BuildObject
	{
		/** @brief WorldBody whose transform defines this object's local frame. */
		WorldBodyId world_body_id = static_cast<WorldBodyId>(-1);
		/** @brief Incarnation token for rejecting work targeting a reused BuildObject ID. */
		uint64 generation_id = 0;
		/** @brief BuildPart IDs owned by this object; IDs are stable while parts are active. */
		DynamicArray<BuildPartId> build_part_ids;
		/** @brief Combined BuildPart mesh in the associated WorldBody's local frame. */
		MeshRenderData detail_mesh;
		/** @brief Origin subtracted from generated vertices to preserve float precision. */
		glm::dvec3 detail_mesh_position = glm::dvec3(0.0);
		/** @brief Revision used to reject stale asynchronous mesh results. */
		uint64 detail_mesh_revision = 1;
		/** @brief Whether the combined structure mesh needs regeneration. */
		bool detail_mesh_dirty = false;
		/** @brief Whether a combined structure mesh job is queued. */
		bool detail_mesh_queued = false;
		/** @brief Local-space conservative sphere for culling the generated mesh. */
		vec3 detail_mesh_bounds_center = vec3(0.0f);
		float detail_mesh_bounds_radius = 0.0f;
		bool detail_mesh_bounds_valid = false;
	};
}
