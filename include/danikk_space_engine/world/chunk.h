#pragma once

#include <danikk_framework/dynamic_array.h>
#include <danikk_engine/mesh.h>

#include <danikk_space_engine/component/point/point.h>
#include <danikk_space_engine/component/graph/graph.h>
#include <danikk_space_engine/world/ground_triangle.h>
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
	 * and triangle IDs. Its mesh is a render cache built only from active GroundTriangle records,
	 * grouped into contiguous material ranges; it does not own source geometry and does not perform
	 * volume-cell face culling. Each triangle contributes three separate vertices with one flat
	 * normal and planar UV projection. Planet streaming metadata uses grid_coordinate.
	 */
	class Chunk
	{
		/** @brief Manually allocated GPU mesh, created lazily from active ground triangles. */
		Mesh* mesh = nullptr;
		/** @brief Draw ranges partitioning the mesh indices by triangle material. */
		DynamicArray<ChunkMaterialRange> material_ranges;
		/** @brief True when bounds describe the most recently built, non-dirty mesh. */
		bool bounds_valid = false;

	public:
		/** @brief Set when geometry or material membership requires GPU mesh reconstruction. */
		bool mesh_dirty = true;
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
		/** @brief Ground triangle IDs rendered and raycast as this chunk's surface; each triangle has one material. */
		DynamicArray<GroundTriangleId> ground_triangle_ids;
		/** @brief Point IDs referenced by this chunk's triangles and point components. */
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
		 * @brief Rebuilds the GPU mesh from active ground triangles and groups indices by material.
		 *
		 * Invalid point references and degenerate triangles are skipped. Vertices are duplicated
		 * per triangle to preserve flat normals; UVs are planar projections selected from the
		 * dominant normal axis. A local bounding sphere is computed from the emitted vertices.
		 * @return Reference to the newly created or updated mesh.
		 */
		Mesh& createMesh();

		/**
		 * @brief Appends a point-component slot ID to this chunk's update list.
		 * @param slot_id Active slot registered by WorldData; duplicates are not checked here.
		 */
		void addPointComponentSlot(PointComponentSlotId slot_id);

		/**
		 * @brief Appends a graph-component slot ID to this chunk's update list.
		 * @param slot_id Active slot registered by WorldData; duplicates are not checked here.
		 */
		void addGraphComponentSlot(GraphComponentSlotId slot_id);

		/**
		 * @brief Invokes active point-component callbacks, then graph-component callbacks.
		 *
		 * Each list's initial length is captured before traversal, so slots appended during a
		 * callback are not ticked until a later call. Inactive recycled IDs are skipped; an ID
		 * outside the corresponding pool's slot range or an invalid external payload is fatal.
		 * @param world World passed to each registered component callback.
		 */
		void tick(World& world);

		/**
		 * @brief Invokes active point-component callbacks, then graph-component callbacks for rendering.
		 *
		 * Each list's initial length is captured before traversal, so additions during a callback
		 * are deferred to the next frame traversal. Inactive recycled IDs are skipped.
		 * @param world World passed to each registered component callback.
		 */
		void frame(World& world);

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

		/** @brief Deletes the owned GPU mesh; `invalidateMesh()` separately clears stale ranges and bounds. */
		void destroyMesh();

		/** @brief Marks the cached mesh and its bounds stale after geometry changes. */
		void invalidateMesh();
		/** @brief Reports whether bounds are valid for the current, non-dirty mesh. */
		bool hasValidBounds() const { return bounds_valid; }
	};
}
