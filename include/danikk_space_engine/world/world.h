#pragma once

#include <danikk_space_engine/default.h>
#include <danikk_space_engine/world_environment.h>
#include <danikk_space_engine/world_physics.h>

#include <danikk_space_engine/world/point.h>
#include <danikk_space_engine/component/point/camera.h>
#include <danikk_space_engine/world/chunk.h>
#include <danikk_space_engine/world/chunk_loader.h>
#include <danikk_space_engine/component/point/local_player_controller.h>
#include <danikk_space_engine/world/data.h>
#include <danikk_framework/dynamic_array.h>

namespace danikk_space_engine
{

	/**
	 * @brief Stores the nearest surface-face or BuildPart hit returned by a raycast.
	 *
	 * A miss leaves `hit` false and the remaining fields at their default values. `face` is a
	 * stable chunk/triangle handle suitable for editing while its owning resources remain active;
	 * the point IDs preserve the triangle's stored winding for callers that need its geometry.
	 */
	struct RaycastHit
	{
		/** @brief True when the ray intersects supported world geometry within the requested range. */
		bool hit = false;
		/** @brief Distance from the ray origin to the nearest hit, in world units. */
		float distance = 0.0f;
		/** @brief Nearest hit position in world coordinates. */
		glm::dvec3 position = glm::dvec3(0.0);
		/** @brief Surface normal transformed into world orientation. */
		vec3 normal = vec3(0.0f);
		/** @brief True when the nearest hit is a BuildPart point component. */
		bool hit_build_part = false;
		/** @brief True when the hit was found in a planet scalar field. */
		bool hit_terrain = false;
		/** @brief WorldBody containing the hit geometry. */
		WorldBodyId body_id = static_cast<WorldBodyId>(-1);
		/** @brief Chunk containing the hit geometry or point component. */
		ChunkId chunk_id = static_cast<ChunkId>(-1);
		/** @brief Point-component slot of the hit BuildPart, valid when hit_build_part is true. */
		PointComponentSlotId build_part_slot_id = static_cast<PointComponentSlotId>(-1);
	};

	/**
	 * @brief Coordinates world state, simulation updates, physics, and geometry queries.
	 *
	 * WorldData owns resource lifetimes. World drives body and chunk component callbacks, defers
	 * destructive point cleanup until callback traversal finishes, then advances WorldPhysics.
	 * ChunkLoader performs CPU-side planet generation asynchronously; completed data is validated
	 * and accepted by the main thread before mesh creation and drawing.
	 */
	class World
	{
		/** @brief Traces a ray through the supplied active bodies and returns their nearest hit. */
		RaycastHit raycastBodies(const WorldBodyId* body_ids, size_t body_count,
			const glm::dvec3& origin, const vec3& direction, float max_distance) const;

	public:
		/** @brief Owns all indexed geometry, chunks, bodies, and component-slot pools. */
		WorldData data;
		/** @brief Samples global gravity and other spatial environment fields. */
		WorldEnvironment environment;
		/** @brief Integrates body motion and resolves physical contacts after component ticks. */
		WorldPhysics physics;
		/** @brief Worker that generates planet chunk data off the main thread. */
		ChunkLoader chunk_loader;
		/** @brief Point IDs waiting until component/chunk iteration ends before destruction. */
		DynamicArray<PointId> pending_point_destruction_ids;
		/** @brief NPC slots awaiting a chunk transition after component iteration. */
		DynamicArray<PointComponentSlotId> pending_npc_chunk_updates;
		/** @brief Slot ID of the active camera point component. */
		PointComponentSlotId camera_component_slot_id = 0;
		/** @brief Slot ID of the active local player controller component. */
		PointComponentSlotId local_player_controller_component_slot_id = 0;
		/** @brief Generation token used to reject worker results from an earlier world lifetime. */
		uint64 chunk_loader_generation_id = 0;
		/** @brief Latest desired chunk-generation work, awaiting submission to ChunkLoader. */
		DynamicArray<ChunkGenerationRequest> chunk_generation_requests;
		/** @brief Set when the queued request array changed and must be sent to the worker. */
		bool chunk_generation_requests_dirty = false;

		/**
		 * @brief Executes current unit orders, updates UnitSystem decisions, then runs world physics.
		 *
		 * Component and chunk ID lists are snapshotted where callbacks may mutate ownership lists.
		 * Deferred point deletion runs after traversal. UnitSystem then performs vision/raycast work
		 * and writes orders that NPC controllers execute on their next component tick.
		 */
		void tick();

		/**
		 * @brief Adds a live point ID once to the queue drained after component traversal.
		 * @param point_id Point to destroy after all current component callbacks complete.
		 * Inactive IDs are ignored and duplicate requests are coalesced.
		 */
		void deferPointDestruction(PointId point_id);

		/**
		 * @brief Replaces queued requests for one planet component and marks the worker queue dirty.
		 * @param component_slot_id Planet component whose previous pending requests are replaced.
		 * @param requests Current immutable request set for that component.
		 */
		void replaceChunkGenerationRequests(
			WorldBodyComponentSlotId component_slot_id,
			const DynamicArray<ChunkGenerationRequest>& requests);

		/**
		 * @brief Removes one queued or pending request matching a component and planet-grid coordinate.
		 * @param component_slot_id Component that owns the request.
		 * @param grid_coordinate Coordinate to cancel or forget.
		 */
		void removeChunkGenerationRequest(WorldBodyComponentSlotId component_slot_id, const glm::ivec3& grid_coordinate);

		/**
		 * @brief Accepts completed generation work, runs frame callbacks, and draws resident meshes.
		 *
		 * Body components update first; changed request snapshots are sent to the
		 * worker; at most one valid generated chunk is accepted and its transient CPU
		 * mesh buffer is uploaded immediately. The method then runs chunk frame callbacks, draws chunks,
		 * and conditionally draws debug overlays and the placement preview.
		 */
		void frame();

		/**
		 * @brief Starts the loader, invalidates older results, resets queues, and creates the base scenario.
		 * Advances the world token while skipping zero, clears queued generation and
		 * deferred-destruction IDs, then calls `startBaseScenario()`, which clears
		 * and repopulates WorldData. Results from an earlier initialization are stale.
		 */
		void init();

		/**
		 * @brief Stops and joins chunk generation, invalidates outstanding results, and releases WorldData.
		 * Advances the world token, clears pending requests, and releases WorldData.
		 * Call before destroying the graphics context so owned chunk meshes can
		 * release their GPU resources safely.
		 */
		void shutdown();

		/**
		 * @brief Finds the nearest hit against terrain or a BuildPart in the world.
		 *
		 * Planet chunks are visited with a 3D grid DDA; other bodies scan their chunk lists. Valid
		 * chunk bounds reject misses before triangle tests. The supplied direction is normalized here.
		 * @param origin Ray origin in world coordinates.
		 * @param direction Ray direction; need not be normalized.
		 * @param max_distance Maximum accepted ray length in world units.
		 * @return Closest hit data, or a default result with `hit == false`.
		 */
		RaycastHit raycast(const glm::dvec3& origin, const vec3& direction, float max_distance = 10000.0f) const;

		/**
		 * @brief Finds the nearest hit against terrain or a BuildPart in one WorldBody.
		 * @param body_id Body that owns the geometry to query.
		 * @param origin Ray origin in world coordinates.
		 * @param direction Ray direction; need not be normalized.
		 * @param max_distance Maximum accepted ray length in world units.
		 * @return Closest hit in the specified body, or a default miss if the body is inactive or no hit was found.
		 */
		RaycastHit raycastBody(WorldBodyId body_id, const glm::dvec3& origin,
			const vec3& direction, float max_distance) const;

		/**
		 * @brief Casts a ray from the active camera along its current forward axis.
		 * @param max_distance Maximum ray length in world units.
		 * @return Closest hit data or a default miss when no active camera is available.
		 */
		RaycastHit raycastFromCamera(float max_distance = 10000.0f) const;

		/** @brief Returns the active camera slot; the stored ID must refer to a live slot. */
		PointComponentSlot& getCameraSlot();

		/** @brief Returns the active camera slot as a read-only reference. */
		const PointComponentSlot& getCameraSlot() const;

		/** @brief Returns the CameraComponent payload associated with `camera_component_slot_id`. */
		CameraComponent& getCameraComponent();

		/** @brief Returns the camera payload as a read-only reference. */
		const CameraComponent& getCameraComponent() const;

		/** @brief Returns the point whose chunk-local position defines the camera origin. */
		Point& getCameraPoint();

		/** @brief Returns the camera point as a read-only reference. */
		const Point& getCameraPoint() const;

		/**
		 * @brief Transforms the camera point through its chunk and world body into world-space coordinates.
		 */
		glm::dvec3 getCameraWorldPosition() const;

		/**
		 * @brief Transforms the chunk origin through its owning world body into world-space coordinates.
		 */
		glm::dvec3 getChunkWorldPosition(ChunkId chunk_id) const;

		/**
		 * @brief Transforms a point’s chunk-local position through the chunk and body transforms.
		 */
		glm::dvec3 getPointWorldPosition(ChunkId chunk_id, PointId point_id) const;

		/** @brief Returns the LocalPlayerControllerComponent from its configured live slot. */
		LocalPlayerControllerComponent& getLocalPlayerControllerComponent();

		/** @brief Returns the local-player controller payload as a read-only reference. */
		const LocalPlayerControllerComponent& getLocalPlayerControllerComponent() const;

	};

	extern World world;
}
