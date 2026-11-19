#pragma once

#ifdef swap
#pragma push_macro("swap")
#undef swap
#include <memory>
#pragma pop_macro("swap")
#else
#include <memory>
#endif

#include <danikk_framework/dynamic_array.h>
#include <danikk_space_engine/config.h>
#include <danikk_space_engine/id.h>
#include <danikk_space_engine/geometry/build_part.h>
#include <danikk_space_engine/component/world_body/planet/terrain.h>

namespace danikk_space_engine
{
	/** @brief Selects which world data source is converted into a chunk mesh. */
	enum class ChunkMeshSource : uint8
	{
		/** @brief Extract an isosurface from a planet scalar field. */
		planet_terrain,
		/** @brief Combine the structural BuildPart instances owned by a chunk. */
		build_parts
	};

	/** @brief One BuildPart snapshot in chunk-local coordinates, safe to read on the worker. */
	struct BuildPartMeshInstance
	{
		BuildPartShape shape = BuildPartShape::cube;
		glm::vec3 position = glm::vec3(0.0f);
		glm::mat4 rotation = glm::mat4(1.0f);
		glm::vec3 size = glm::vec3(1.0f);
	};

	/** @brief Immutable planet-field data shared by all chunk jobs for one field revision. */
	struct PlanetTerrainSnapshot
	{
		WorldBodyId world_body_id = static_cast<WorldBodyId>(-1);
		uint64 planet_generation_id = 0;
		uint64 terrain_revision = 0;
		double planet_radius = 0.0;
		double core_radius = 0.0;
		uint64 noise_seed = 0;
		DynamicArray<PlanetTerrainEdit> terrain_edits;
	};

	/** @brief Immutable CPU-only input copied into the mesh generator's worker queue. */
	struct ChunkMeshGenerationRequest
	{
		uint64 generation_id = 0;
		uint64 source_revision = 0;
		WorldBodyId world_body_id = 0;
		ChunkId chunk_id = static_cast<ChunkId>(-1);
		uint64 planet_generation_id = 0;
		uint64 terrain_revision = 0;
		ChunkMeshSource source = ChunkMeshSource::planet_terrain;
		PlanetMeshGenerator planet_mesh_generator = PlanetMeshGenerator::marching_tetrahedra;
		double chunk_width = 32.0;
		double voxel_size = 4.0;
		glm::ivec3 grid_coordinate = glm::ivec3(0);
		glm::dvec3 chunk_position = glm::dvec3(0.0);
		std::shared_ptr<const PlanetTerrainSnapshot> planet_terrain;
		DynamicArray<BuildPartMeshInstance> build_parts;
	};

	/** @brief CPU mesh payload returned to the main thread for validation and GPU upload. */
	struct GeneratedChunkMesh
	{
		uint64 generation_id = 0;
		uint64 source_revision = 0;
		WorldBodyId world_body_id = 0;
		ChunkId chunk_id = static_cast<ChunkId>(-1);
		uint64 planet_generation_id = 0;
		uint64 terrain_revision = 0;
		ChunkMeshSource source = ChunkMeshSource::planet_terrain;
		PlanetMeshGenerator planet_mesh_generator = PlanetMeshGenerator::marching_tetrahedra;
		glm::ivec3 grid_coordinate = glm::ivec3(0);
		glm::dvec3 chunk_position = glm::dvec3(0.0);
		DynamicArray<vec3> point_positions;
		DynamicArray<uint32> triangle_point_indices;
		DynamicArray<MaterialId> triangle_material_ids;
	};

	/** @brief Starts the process-wide CPU mesh-generation worker. Safe to call repeatedly. */
	void startChunkMeshGenerator();
	/** @brief Stops and joins the worker, discarding pending and completed jobs. */
	void stopChunkMeshGenerator();
	/** @brief Replaces pending work with the latest immutable world request snapshot. */
	void updateChunkMeshGenerationRequests(uint64 generation_id,
		const DynamicArray<ChunkMeshGenerationRequest>& requests);
	/** @brief Removes queued/completed work for one planet incarnation and suppresses its active result. */
	void cancelChunkMeshGenerationRequests(WorldBodyId world_body_id, uint64 planet_generation_id);
	/** @brief Takes the oldest completed CPU mesh, if one is available. */
	bool tryTakeGeneratedChunkMesh(GeneratedChunkMesh& result);
}
