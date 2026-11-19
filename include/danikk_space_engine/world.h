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
#include <danikk_space_engine/planet_system.h>
#include <danikk_space_engine/raycast_system.h>
#include <danikk_space_engine/factions.h>
#include <danikk_framework/dynamic_array.h>

namespace danikk_space_engine
{

	/**
	 * @brief Coordinates world state, simulation updates, physics, and geometry queries.
	 *
	 * PointData and WorldBodyData own resource lifetimes. World coordinates chunk membership,
	 * component callbacks, deferred
	 * destructive point cleanup until callback traversal finishes, then advances PhysicsSystem.
	 * ChunkMeshGenerator performs CPU-side mesh generation asynchronously; completed data is validated
	 * and accepted by the main thread before mesh creation and drawing.
	 */
	class World
	{
	public:
		/** @brief Owns point resources and their component slots. */
		PointData point_data;
		/** @brief Owns WorldBody resources and their component slots. */
		WorldBodyData world_body_data;
		/** @brief Owns chunk data, chunk memberships, and chunk meshes. */
		ChunkSystem chunks;
		/** @brief Samples global gravity and other spatial environment fields. */
		EnvironmentSystem environment;
		/** @brief Integrates body motion and resolves physical contacts after component ticks. */
		PhysicsSystem physics;
		/** @brief Processes local player input and stores its selected point and interaction state. */
		ControllerSystem controller;
		/** @brief Dispatches tick and frame callbacks for WorldBody and point components. */
		ComponentSystem components;
		/** @brief Owns planet data, terrain queries, edits, and chunk streaming. */
		PlanetSystem planets;
		/** @brief Performs terrain and BuildPart ray queries. */
		RaycastSystem raycasts;
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
		 * performs vision/raycast work, and assigns the next orders before PhysicsSystem advances.
		 */
		void tick();

		/**
		 * @brief Adds a live point ID once to the queue drained after component traversal.
		 * @param point_id Point to destroy after all current component callbacks complete.
		 * Inactive IDs are ignored and duplicate requests are coalesced.
		 */
		void deferPointDestruction(PointId point_id);

		/** @brief Snapshots dirty BuildPart chunks into CPU-only mesh generation jobs. */
		void queueDirtyBuildPartMeshes();

		/**
		 * @brief Replaces queued requests for one planet and marks the worker queue dirty.
		 * @param world_body_id Planet body owning the requests.
		 * @param planet_generation_id Planet incarnation used to reject stale work.
		 * @param requests Current immutable request set for that planet.
		 */
		void replaceChunkMeshGenerationRequests(
			WorldBodyId world_body_id, uint64 planet_generation_id,
			const DynamicArray<ChunkMeshGenerationRequest>& requests);

		/**
		 * @brief Removes one queued or pending request matching a component and planet-grid coordinate.
		 * @param world_body_id Planet body that owns the request.
		 * @param planet_generation_id Planet incarnation that owns the request.
		 * @param grid_coordinate Coordinate to cancel or forget.
		 */
		void removeChunkMeshGenerationRequest(WorldBodyId world_body_id,
			uint64 planet_generation_id, const glm::ivec3& grid_coordinate);
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

}
