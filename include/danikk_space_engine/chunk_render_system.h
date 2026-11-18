#pragma once

#include <danikk_framework/dynamic_array.h>
#include <danikk_engine/mesh.h>

#include <danikk_space_engine/world/chunk.h>

namespace danikk_space_engine
{
	/** @brief Material range in a chunk mesh's shared index buffer. */
	struct ChunkMaterialRange
	{
		MaterialId material_id = default_materials::void_material;
		uint32 first_index = 0;
		uint32 index_count = 0;
	};

	/** @brief GPU resources and draw metadata for one active chunk. */
	struct ChunkRenderRecord
	{
		Mesh* mesh = nullptr;
		DynamicArray<ChunkMaterialRange> material_ranges;
		bool has_build_part_mesh = false;
	};

	/**
	 * @brief Owns, uploads, draws, and releases GPU meshes for world chunks.
	 *
	 * Records are indexed by stable ChunkId. Chunk geometry is consumed synchronously during
	 * upload; only GPU data and material draw ranges are retained here.
	 */
	class ChunkRenderSystem
	{
		DynamicArray<ChunkRenderRecord> records;

		/** @brief Returns the record for a chunk, expanding the indexed record array if needed. */
		ChunkRenderRecord& getRecord(ChunkId chunk_id);
		/** @brief Returns null when no render record exists for the requested chunk. */
		const ChunkRenderRecord* findRecord(ChunkId chunk_id) const;

	public:
		/** @brief Uploads temporary triangle geometry and updates the chunk's spatial bounds. */
		void createMesh(Chunk& chunk, const vec3* positions, size_t position_count,
			const uint32* triangle_indices, size_t index_count,
			const MaterialId* triangle_material_ids, size_t material_count,
			bool is_planet_terrain = true, bool is_build_part_mesh = false);

		/** @brief Draws the chunk mesh using its material ranges and camera-relative transform. */
		void draw(const Chunk& chunk, World& world);

		/** @brief Releases one chunk's GPU resource and clears its render metadata. */
		void destroyChunk(Chunk& chunk);
		/** @brief Releases all GPU resources while the graphics context is active. */
		void clear();
		/** @brief Returns whether the chunk has a combined BuildPart mesh. */
		bool hasBuildPartMesh(ChunkId chunk_id) const;
	};
}
