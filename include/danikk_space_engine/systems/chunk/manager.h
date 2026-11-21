#pragma once

#include <danikk_framework/dynamic_array.h>
#include <danikk_engine/mesh.h>

#include <danikk_space_engine/systems/chunk/chunk.h>
#include <danikk_space_engine/resource_pool.h>
#include <danikk_space_engine/mesh_render_data.h>

namespace danikk_space_engine
{
	/** @brief GPU mesh and draw metadata owned alongside one active chunk. */
	struct ChunkRenderRecord : MeshRenderData
	{
		bool build_part_mesh_dirty = false;
		bool render_enabled = true;
		bool has_build_part_mesh = false;
		bool bounds_valid = false;
		uint64 build_part_mesh_revision = 0;
		vec3 bounds_center = vec3(0.0f);
		float bounds_radius = 0.0f;
	};

	/**
	 * @brief Owns chunk data, chunk-to-world membership, and chunk GPU meshes.
	 *
	 * PointData owns points and point components; WorldBodyData owns bodies and their components.
	 * ChunkSystem owns per-cell membership indices and GPU meshes, coordinates changes to those
	 * records, and releases GPU resources before recycling chunk IDs.
	 */
	class ChunkSystem
	{
		/** @brief Owning World whose data and callbacks this system manages. */
		World& world;
		/** @brief Stable-ID pool for chunk spatial data and membership lists. */
		WorldResourcePool<Chunk, ChunkId> chunks{"Chunk"};
		/** @brief Point references indexed by chunk ID, kept outside the Chunk records. */
		DynamicArray<DynamicArray<PointId>> point_ids_by_chunk;
		/** @brief BuildPart slots indexed by chunk ID while component scheduling is still cell-based. */
		DynamicArray<DynamicArray<PointComponentSlotId>> build_part_slots_by_chunk;
		/** @brief GPU resources and material ranges indexed by stable ChunkId. */
		DynamicArray<ChunkRenderRecord> render_records;
		/** @brief Last timestamp used to report chunk draw statistics. */
		uint32 render_report_timestamp = 0;
		/** @brief Chunk draw outcomes accumulated since the last statistics report. */
		uint64 render_candidates = 0;
		uint64 render_drawn = 0;
		uint64 render_missing_mesh = 0;
		uint64 render_empty_mesh = 0;
		uint64 render_inactive = 0;

		/** @brief Returns the GPU record for a chunk, expanding the indexed array if needed. */
		ChunkRenderRecord& getRenderRecord(ChunkId chunk_id);
		/** @brief Returns null when no GPU record exists for the requested chunk. */
		const ChunkRenderRecord* findRenderRecord(ChunkId chunk_id) const;

	public:
		/** @brief Binds this chunk system to its owning World. */
		explicit ChunkSystem(World& world) : world(world) {}
		/** @brief Logs initialization of the chunk subsystem. */
		void init();
		/** @brief Releases chunk resources and logs subsystem shutdown. */
		void shutdown();

		/** @brief Creates an empty chunk and registers it with its owning WorldBody. */
		Chunk& createChunk(WorldBodyId world_body_id);
		/** @brief Returns whether an ID currently refers to an active chunk. */
		bool isChunkActive(ChunkId id) const;
		/** @brief Returns active chunk IDs; do not retain the reference across pool mutations. */
		const DynamicArray<ChunkId>& getUsedChunkIds() const;
		/** @brief Returns the total number of allocated chunk slots, including reusable slots. */
		size_t getChunkSlotCount() const;
		/** @brief Returns point IDs spatially referenced by an active chunk. */
		const DynamicArray<PointId>& getChunkPointIds(ChunkId chunk_id) const;
		/** @brief Returns BuildPart component slots assigned to an active spatial cell. */
		const DynamicArray<PointComponentSlotId>& getBuildPartSlotIds(ChunkId chunk_id) const;
		/** @brief Invalidates the combined BuildPart mesh for a spatial cell. */
		void invalidateBuildPartMesh(ChunkId chunk_id);
		/** @brief Returns whether the current BuildPart mesh is queued for regeneration. */
		bool isBuildPartMeshDirty(ChunkId chunk_id) const;
		/** @brief Returns the revision used to reject outdated BuildPart mesh jobs. */
		uint64 getBuildPartMeshRevision(ChunkId chunk_id) const;
		/** @brief Marks the current BuildPart mesh revision as submitted to the worker. */
		void markBuildPartMeshQueued(ChunkId chunk_id);
		/** @brief Enables or disables rendering for one spatial cell. */
		void setRenderEnabled(ChunkId chunk_id, bool enabled);
		/** @brief Returns whether a spatial cell has usable bounds; outputs its local sphere. */
		bool getChunkBounds(ChunkId chunk_id, vec3& center, float& radius) const;
		/** @brief Returns whether a spatial cell's mesh is enabled for rendering. */
		bool isRenderEnabled(ChunkId chunk_id) const;
		/** @brief Returns a checked reference to an active chunk. */
		Chunk& getChunk(ChunkId id);
		/** @brief Returns a checked read-only reference to an active chunk. */
		const Chunk& getChunk(ChunkId id) const;
		/** @brief Transforms a chunk's body-local origin into world-space coordinates. */
		glm::dvec3 getChunkWorldPosition(ChunkId chunk_id) const;

		/** @brief Adds a point reference to a chunk and tracks the reverse membership on the point. */
		void addPointReferenceToChunk(ChunkId chunk_id, PointId point_id);
		/** @brief Removes a point reference from a chunk without destroying the point. */
		bool removePointReferenceFromChunk(ChunkId chunk_id, PointId point_id);
		/** @brief Attaches a point component to its scheduling chunk and point reference. */
		void addPointComponentSlotToChunk(ChunkId chunk_id, PointComponentSlotId slot_id);
		/** @brief Detaches a point component slot from its chunk while retaining its payload. */
		void removePointComponentSlotFromChunk(PointComponentSlotId slot_id);
		/** @brief Destroys a point and removes all its chunk references before freeing its payloads. */
		void destroyPoint(PointId point_id);
		/** @brief Destroys a chunk, its GPU mesh, attached components, and unreferenced points. */
		void destroyChunk(ChunkId chunk_id);
		/** @brief Destroys every active chunk and releases each associated GPU resource. */
		void clear();

		/** @brief Converts transient triangles into a flat-shaded, material-grouped GPU mesh. */
		void createMesh(Chunk& chunk, const vec3* positions, size_t position_count,
			const uint32* triangle_indices, size_t index_count,
			const MaterialId* triangle_material_ids, size_t material_count,
			bool is_build_part_mesh = false);
		/** @brief Draws the chunk's material ranges and reports why drawing did not occur. */
		MeshDrawResult draw(const Chunk& chunk);
		/** @brief Draws chunk meshes using the world's one- or two-depth-pass setup. */
		void renderWorld();
		/** @brief Releases one chunk's GPU mesh and clears its bounds and render metadata. */
		void destroyRenderData(Chunk& chunk);
		/** @brief Releases all GPU mesh resources while the graphics context is active. */
		void clearRenderData();
		/** @brief Returns whether the chunk currently has a combined BuildPart mesh. */
		bool hasBuildPartMesh(ChunkId chunk_id) const;
	};
}
