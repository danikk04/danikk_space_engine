#pragma once

#include <danikk_framework/dynamic_array.h>
#include <danikk_engine/mesh.h>

#include <danikk_space_engine/component/point/point.h>
#include <danikk_space_engine/component/graph/graph.h>
#include <danikk_space_engine/id.h>
#include <danikk_space_engine/material.h>
#include <danikk_space_engine/fwd.h>

namespace danikk_space_engine
{

	/**
	 * @brief Maps a contiguous index range in a chunk mesh to one material.
	 *
	 * Ranges are emitted in ascending MaterialId order and cover only materials with at least one
	 * valid triangle. The index offsets address the shared mesh element buffer.
	 */
	struct ChunkMaterialRange
	{
		/** @brief Material bound before drawing this contiguous index range. */
		MaterialId material_id = default_materials::void_material;
		/** @brief First index in the chunk mesh's element buffer for this material. */
		uint32 first_index = 0;
		/** @brief Number of element indices belonging to this material. */
		uint32 index_count = 0;
	};

	/**
	 * @brief Owns local surface geometry, component-slot lists, and one lazily built GPU mesh.
	 *
	 * A Chunk is positioned relative to its WorldBody and references pooled point
	 * and component lists. Planet meshes are uploaded from temporary generated buffers and retained
	 * only by the GPU mesh; the chunk does not keep terrain vertices or triangles in WorldData.
	 * Planet streaming metadata uses grid_coordinate.
	 */
	class Chunk
	{
		friend class WorldData;

		/** @brief Manually allocated GPU mesh, created lazily from active ground triangles. */
		Mesh* mesh = nullptr;
		/** @brief Draw ranges partitioning the mesh indices by triangle material. */
		DynamicArray<ChunkMaterialRange> material_ranges;
		/** @brief True when bounds describe the most recently built, non-dirty mesh. */
		bool bounds_valid = false;
		/** @brief True when this chunk represents a streamed planet terrain cell. */
		bool planet_terrain = false;
		/** @brief True when the GPU mesh contains the combined geometry of this chunk's BuildParts. */
		bool has_build_part_mesh = false;
		/** @brief Subset of the point-component list containing only human slots. */
		DynamicArray<PointComponentSlotId> human_component_slot_ids;

		/** @brief Registers a unique slot and updates the human subset when applicable. */
		void addPointComponentSlot(PointComponentSlotId slot_id, PointComponentTypeId type_id);

		/** @brief Removes an owned slot from the update list and, for humans, from the human subset. */
		void removePointComponentSlot(PointComponentSlotId slot_id, PointComponentTypeId type_id);

	public:
		/** @brief Whether at least one human in this chunk needs checks during the current collision tick. */
		bool has_active_human_collisions = false;
		/** @brief Whether the combined BuildPart mesh needs regeneration. */
		bool build_part_mesh_dirty = false;
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
		/** @brief Point IDs referenced by this chunk's point and graph components. */
		DynamicArray<PointId> point_ids;
		/** @brief Point-attached component slots ticked and framed by this chunk. */
		DynamicArray<PointComponentSlotId> point_component_slot_ids;
		/** @brief Edge-attached component slots ticked and framed by this chunk. */
		DynamicArray<GraphComponentSlotId> graph_component_slot_ids;

		/** @brief Creates an empty chunk resource. */
		Chunk() = default;
		/** @brief Copying is disabled because the chunk owns a GPU mesh and pooled resource IDs. */
		Chunk(const Chunk&) = delete;
		/** @brief Copy assignment is disabled for the same ownership reason as copy construction. */
		Chunk& operator=(const Chunk&) = delete;
		/** @brief Transfers the mesh handle, material ranges, and resource lists from another chunk. */
		Chunk(Chunk&& other) noexcept;
		/** @brief Releases this chunk's current mesh before taking ownership of another chunk's data. */
		Chunk& operator=(Chunk&& other) noexcept;
		/** @brief Destroys the owned GPU mesh if it has been created. */
		~Chunk();

		/**
		 * @brief Uploads transient indexed geometry into this chunk's GPU mesh.
	 *
	 * Inputs are consumed synchronously and are not retained. Vertices are duplicated per
	 * triangle for flat normals and grouped into material ranges. Bounds are computed from the
	 * emitted vertices for raycast broadphase.
	 * @param positions Chunk-local source positions.
	 * @param position_count Number of source positions.
	 * @param triangle_indices Three source-position indices per triangle.
	 * @param index_count Number of indices; must be divisible by three.
	 * @param triangle_material_ids One material ID per triangle.
	 * @param material_count Number of triangle material IDs.
		 */
		void createMesh(const vec3* positions, size_t position_count,
			const uint32* triangle_indices, size_t index_count,
			const MaterialId* triangle_material_ids, size_t material_count,
			bool is_planet_terrain = true, bool is_build_part_mesh = false);

		/** @brief Invalidates the combined BuildPart mesh and advances its source revision. */
		void invalidateBuildPartMesh();

		/** @brief Returns the human component IDs in the chunk's update order. */
		const DynamicArray<PointComponentSlotId>& getHumanComponentSlotIds() const
		{
			return human_component_slot_ids;
		}

		/**
		 * @brief Appends a graph-component slot ID to this chunk's update list.
		 * @param slot_id Active slot registered by WorldData; duplicates are not checked here.
		 */
		void addGraphComponentSlot(GraphComponentSlotId slot_id);

		/**
		 * @brief Builds the chunk-to-camera-relative render transform from body and chunk transforms.
		 * @param world World providing the owning body transform and camera position.
		 * @return Matrix mapping chunk-local coordinates into camera-relative render coordinates.
		 */
		mat4 createWorldMatrix(const World& world) const;

		/**
		 * @brief Draws each non-empty material range of the built chunk mesh.
		 *
		 * The camera-relative transform is set once, then each range binds its
		 * material texture (or the engine's white fallback) and RGB tint before
		 * issuing an indexed draw. A missing mesh or an empty range list is a no-op.
		 * @param world World supplying the owning body transform and camera position.
		 */
		void draw(World& world);

		/** @brief Deletes the owned GPU mesh and clears its material ranges and bounds. */
		void destroyMesh();
		/** @brief Reports whether bounds are valid for the current, non-dirty mesh. */
		bool hasValidBounds() const { return bounds_valid; }
		/** @brief Reports whether this chunk stores a streamed planet-terrain mesh. */
		bool isPlanetTerrain() const { return planet_terrain; }
		/** @brief Reports whether the GPU mesh replaces individual BuildPart rendering. */
		bool hasBuildPartMesh() const { return has_build_part_mesh; }
	};
}
