#pragma once

#include <danikk_space_engine/default.h>
#include <danikk_space_engine/environment.h>
#include <danikk_space_engine/physics.h>

#include <danikk_space_engine/point.h>
#include <danikk_space_engine/component/point/camera.h>
#include <danikk_space_engine/chunk.h>
#include <danikk_space_engine/chunk_mesh_generator.h>
#include <danikk_space_engine/controller_system.h>
#include <danikk_space_engine/component_system.h>
#include <danikk_space_engine/world_body.h>
#include <danikk_space_engine/chunk_system.h>
#include <danikk_space_engine/planet_terrain_system.h>
#include <danikk_space_engine/factions.h>
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
	 * PointData and WorldBodyData own resource lifetimes. World coordinates chunk membership,
	 * component callbacks, deferred
	 * destructive point cleanup until callback traversal finishes, then advances WorldPhysics.
	 * ChunkMeshGenerator performs CPU-side mesh generation asynchronously; completed data is validated
	 * and accepted by the main thread before mesh creation and drawing.
	 */
	class World
	{
		/** @brief Traces a ray through the supplied active bodies and returns their nearest hit. */
		RaycastHit raycastBodies(const WorldBodyId* body_ids, size_t body_count,
			const glm::dvec3& origin, const vec3& direction, float max_distance) const;

	public:
		/** @brief Owns point resources and their component slots. */
		PointData point_data;
		/** @brief Owns WorldBody resources and their component slots. */
		WorldBodyData world_body_data;
		/** @brief Owns chunk data, chunk memberships, and chunk meshes. */
		ChunkSystem chunk_system;
		/** @brief Samples global gravity and other spatial environment fields. */
		WorldEnvironment environment;
		/** @brief Integrates body motion and resolves physical contacts after component ticks. */
		WorldPhysics physics;
		/** @brief Processes local player input and stores its selected point and interaction state. */
		ControllerSystem controller_system;
		/** @brief Dispatches tick and frame callbacks for WorldBody and point components. */
		ComponentSystem component_system;
		/** @brief Samples live planet density and material fields. */
		PlanetTerrainSystem planet_terrain_system;
		/** @brief Owns this world's faction instances and diplomatic relations. */
		FactionSystem factions;
		/** @brief Point IDs waiting until component/chunk iteration ends before destruction. */
		DynamicArray<PointId> pending_point_destruction_ids;
		/** @brief Slot ID of the active camera point component. */
		PointComponentSlotId camera_component_slot_id = 0;
		/** @brief Generation token used to reject worker results from an earlier world lifetime. */
		uint64 chunk_mesh_generator_generation_id = 0;
		/** @brief Latest desired chunk-mesh work, awaiting submission to the generator module. */
		DynamicArray<ChunkMeshGenerationRequest> chunk_generation_requests;
		/** @brief Set when the queued request array changed and must be sent to the worker. */
		bool chunk_generation_requests_dirty = false;

		/** @brief Constructs subsystems bound to this World instance. */
		World();
		World(const World&) = delete;
		World& operator=(const World&) = delete;

		/**
		 * @brief Executes current unit orders, updates UnitSystem decisions, then runs world physics.
		 *
		 * Component and chunk ID lists are snapshotted where callbacks may mutate ownership lists.
		 * Deferred point deletion runs after traversal. UnitSystem then executes stored agent orders,
		 * performs vision/raycast work, and assigns the next orders before WorldPhysics advances.
		 */
		void tick();

		/**
		 * @brief Adds a live point ID once to the queue drained after component traversal.
		 * @param point_id Point to destroy after all current component callbacks complete.
		 * Inactive IDs are ignored and duplicate requests are coalesced.
		 */
		void deferPointDestruction(PointId point_id);

		/** @brief Cancels generation work owned by the body's component slots, then destroys the body. */
		void destroyWorldBody(WorldBodyId body_id);
		/** @brief Releases a chunk's render resources, then destroys the chunk resource. */
		void destroyChunk(ChunkId chunk_id);
		/** @brief Detaches and destroys a point together with its component payloads. */
		void destroyPoint(PointId point_id);

		/** @brief Snapshots dirty BuildPart chunks into CPU-only mesh generation jobs. */
		void queueDirtyBuildPartMeshes();

		/**
		 * @brief Replaces queued requests for one planet component and marks the worker queue dirty.
		 * @param component_slot_id Planet component whose previous pending requests are replaced.
		 * @param requests Current immutable request set for that component.
		 */
		void replaceChunkMeshGenerationRequests(
			WorldBodyComponentSlotId component_slot_id,
			const DynamicArray<ChunkMeshGenerationRequest>& requests);

		/**
		 * @brief Removes one queued or pending request matching a component and planet-grid coordinate.
		 * @param component_slot_id Component that owns the request.
		 * @param grid_coordinate Coordinate to cancel or forget.
		 */
		void removeChunkMeshGenerationRequest(WorldBodyComponentSlotId component_slot_id, const glm::ivec3& grid_coordinate);
		/** @brief Removes the queued BuildPart mesh job for one chunk. */
		void removeBuildPartMeshRequest(ChunkId chunk_id);

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
		 * and repopulates the point and body data pools. Results from an earlier initialization are stale.
		 */
		void init();

		/**
		 * @brief Stops and joins chunk generation, invalidates outstanding results, and releases world data.
		 * Advances the world token, clears pending requests, and releases the point and body pools.
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

	};

	extern World world;
}
