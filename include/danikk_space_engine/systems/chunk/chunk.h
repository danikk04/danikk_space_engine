#pragma once

#include <danikk_framework/dynamic_array.h>

#include <danikk_space_engine/component/point/point.h>
#include <danikk_space_engine/id.h>
#include <danikk_space_engine/material.h>
#include <danikk_space_engine/fwd.h>

namespace danikk_space_engine { class ChunkSystem; }

namespace danikk_space_engine
{

	/**
	 * @brief Stores spatial point references and BuildPart geometry for one chunk.
	 *
	 * A Chunk is positioned relative to its WorldBody and references pooled point
	 * and BuildPart lists. Other point components live in PointData, not duplicated here.
	 * Render meshes are owned by ChunkSystem; the chunk keeps only
	 * spatial bounds used by raycast broadphase.
	 * Planet streaming metadata uses grid_coordinate.
	 */
	class Chunk
	{
		friend class ChunkSystem;

		/** @brief True when bounds describe the most recently built, non-dirty mesh. */
		bool bounds_valid = false;
		/** @brief Registers a BuildPart slot used by this chunk's mesh and collision geometry. */
		void addBuildPartComponentSlot(PointComponentSlotId slot_id);

		/** @brief Removes a BuildPart slot from this chunk's geometry list. */
		void removeBuildPartComponentSlot(PointComponentSlotId slot_id);

	public:
		/** @brief Whether the combined BuildPart mesh needs regeneration. */
		bool build_part_mesh_dirty = false;
		/** @brief Whether this mesh is currently selected for rendering. */
		bool render_enabled = true;
		/** @brief Revision advanced whenever a BuildPart in the chunk changes. */
		uint64 build_part_mesh_revision = 0;
		/** @brief Center of the local-space bounding sphere used by raycast broadphase. */
		vec3 bounds_center = vec3(0.0f);
		/** @brief Radius of the local-space bounding sphere used by raycast broadphase. */
		float bounds_radius = 0.0f;
		/** @brief Stable pooled chunk ID. */
		ChunkId chunk_id = 0;
		/** @brief World body that supplies this chunk's parent transform. */
		WorldBodyId world_body_id = 0;
		/** @brief Integer grid coordinate for streamed planet chunks. */
		glm::ivec3 grid_coordinate = glm::ivec3(0);
		/** @brief Chunk origin relative to its WorldBody origin, in double precision. */
		glm::dvec3 position = glm::dvec3(0.0);
		/** @brief Point IDs referenced by this chunk and its point components. */
		DynamicArray<PointId> point_ids;
		/** @brief BuildPart components whose combined geometry belongs to this chunk. */
		DynamicArray<PointComponentSlotId> build_part_component_slot_ids;

		/** @brief Creates an empty chunk resource. */
		Chunk() = default;
		/** @brief Copying is disabled because the chunk represents a unique pooled resource. */
		Chunk(const Chunk&) = delete;
		/** @brief Copy assignment is disabled for the same ownership reason as copy construction. */
		Chunk& operator=(const Chunk&) = delete;
		/** @brief Transfers spatial data and resource lists from another chunk. */
		Chunk(Chunk&& other) noexcept;
		/** @brief Replaces this chunk's spatial data and resource lists. */
		Chunk& operator=(Chunk&& other) noexcept;
		/** @brief Destroys CPU-side chunk data. */
		~Chunk() = default;

		/** @brief Invalidates the combined BuildPart mesh and advances its source revision. */
		void invalidateBuildPartMesh();

		/**
		 * @brief Builds the chunk-to-camera-relative render transform from body and chunk transforms.
		 * @param world World providing the owning body transform and camera position.
		 * @return Matrix mapping chunk-local coordinates into camera-relative render coordinates.
		 */
		mat4 createWorldMatrix(const World& world) const;

		/** @brief Reports whether bounds are valid for the current, non-dirty mesh. */
		bool hasValidBounds() const { return bounds_valid; }
	};
}
