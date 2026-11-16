#pragma once

#include <danikk_space_engine/id.h>
#include <danikk_space_engine/component/world_body/world_body.h>
#include <danikk_space_engine/default.h>
#include <danikk_framework/dynamic_array.h>
#include <danikk_space_engine/component/world_body/planet/terrain.h>
#include <glm/glm.hpp>

namespace danikk_space_engine
{

	/**
	 * @brief Immutable input snapshot identifying one planet chunk generation job.
	 *
	 * The worker receives this value rather than reading live WorldData. Its token
	 * fields allow the main thread to reject results from an old world, component
	 * incarnation, or terrain revision. `terrain_edits` is copied with the request
	 * so a generation job sees a consistent edit set.
	 */
	struct ChunkGenerationRequest
	{
		/** @brief Unique job ID used to match the worker result to its request. */
		uint64 generation_id = 0;
		/** @brief World body whose terrain is being sampled. */
		WorldBodyId world_body_id = 0;
		/** @brief Planet component slot that submitted this request. */
		WorldBodyComponentSlotId component_slot_id = 0;
		/** @brief Component incarnation token used to reject results for a replaced component. */
		uint64 component_generation_id = 0;
		/** @brief Terrain revision captured when this request was queued. */
		uint64 terrain_revision = 0;
		/** @brief Planet radius snapshot used by the worker. */
		double planet_radius = 0.0;
		/** @brief Core radius snapshot used by the worker. */
		double core_radius = 0.0;
		/** @brief Edge length of the chunk-generation region in world units. */
		double chunk_width = 32.0;
		/** @brief Spacing between scalar-field samples in world units. */
		double voxel_size = 4.0;
		/** @brief Procedural noise seed captured for deterministic output. */
		uint64 noise_seed = 0;
		/** @brief Integer coordinate in the planet's chunk grid. */
		glm::ivec3 grid_coordinate = glm::ivec3(0);
		/** @brief Chunk origin in world coordinates relative to the planet center. */
		glm::dvec3 chunk_position = glm::dvec3(0.0);
		/** @brief Immutable snapshot of terrain stamps used for this generation job. */
		DynamicArray<PlanetTerrainEdit> terrain_edits;
	};

	/**
	 * @brief Holds worker-generated CPU mesh data before WorldData or GPU resources are created.
	 *
	 * `triangle_point_indices` contains three indices per triangle, indexing
	 * `point_positions`. When present, `triangle_material_ids` contains one
	 * material ID per triangle. The request identity fields are copied from the
	 * generation request and checked before the main thread installs the result.
	 */
	struct GeneratedChunkData
	{
		/** @brief Job ID copied from the request. */
		uint64 generation_id = 0;
		/** @brief World body that should receive the generated geometry. */
		WorldBodyId world_body_id = 0;
		/** @brief Component slot expected to accept the worker result. */
		WorldBodyComponentSlotId component_slot_id = 0;
		/** @brief Component incarnation token copied from the request. */
		uint64 component_generation_id = 0;
		/** @brief Terrain revision represented by the generated geometry. */
		uint64 terrain_revision = 0;
		/** @brief Planet grid coordinate represented by this generated result. */
		glm::ivec3 grid_coordinate = glm::ivec3(0);
		/** @brief Chunk origin relative to the planet center. */
		glm::dvec3 chunk_position = glm::dvec3(0.0);
		/** @brief Generated vertices expressed relative to the chunk origin. */
		DynamicArray<vec3> point_positions;
		/** @brief Consecutive triples of indices into `point_positions`, one triple per triangle. */
		DynamicArray<uint32> triangle_point_indices;
		/** @brief Material ID parallel to each generated triangle. */
		DynamicArray<MaterialId> triangle_material_ids;
	};

	/**
	 * @brief Loads a compatible saved chunk or samples the scalar field and extracts its isosurface.
	 *
	 * Generation uses marching tetrahedra on the request's voxel grid, then returns chunk-local
	 * vertices, triangle indices, and per-triangle material IDs without touching WorldData or GPU state.
	 * @param request Immutable parameters, edit snapshot, and coordinate for one chunk job.
	 * @return Worker result carrying the request identity and generated mesh data; may be empty.
	 */
	GeneratedChunkData generatePlanetChunk(const ChunkGenerationRequest& request);
}
