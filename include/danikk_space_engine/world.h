#pragma once

#include <danikk_space_engine/default.h>
#include <danikk_space_engine/environment.h>
#include <danikk_space_engine/physics.h>

#include <danikk_space_engine/point.h>
#include <danikk_space_engine/camera_system.h>
#include <danikk_space_engine/chunk.h>
#include <danikk_space_engine/controller_system.h>
#include <danikk_space_engine/component_system.h>
#include <danikk_space_engine/world_body.h>
#include <danikk_space_engine/chunk_system.h>
#include <danikk_space_engine/chunk_mesh_generation_system.h>
#include <danikk_space_engine/planet_system.h>
#include <danikk_space_engine/raycast_system.h>
#include <danikk_space_engine/factions.h>
#include <danikk_space_engine/unit_system.h>

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
		/** @brief Owns asynchronous chunk mesh requests and result acceptance. */
		ChunkMeshGenerationSystem chunk_mesh_generation;
		/** @brief Samples global gravity and other spatial environment fields. */
		EnvironmentSystem environment;
		/** @brief Integrates body motion and resolves physical contacts after component ticks. */
		PhysicsSystem physics;
		/** @brief Processes local player input and stores its selected point and interaction state. */
		ControllerSystem controller;
		/** @brief Owns camera state, input, point binding, and projection. */
		CameraSystem camera;
		/** @brief Dispatches tick and frame callbacks for WorldBody and point components. */
		ComponentSystem components;
		/** @brief Owns planet data, terrain queries, edits, and chunk streaming. */
		PlanetSystem planets;
		/** @brief Performs terrain and BuildPart ray queries. */
		RaycastSystem raycasts;
		/** @brief Owns this world's faction instances and diplomatic relations. */
		FactionSystem factions;
		/** @brief Owns per-faction unit groups, tactical memory, and orders. */
		UnitSystem unit_system;

		/** @brief Constructs subsystems bound to this World instance. */
		World();
		World(const World&) = delete;
		World& operator=(const World&) = delete;

		/**
		 * @brief Executes current unit orders, updates UnitGroup decisions, then runs world physics.
		 *
		 * Component and chunk ID lists are snapshotted where callbacks may mutate ownership lists.
		 * Deferred point deletion is flushed after component and controller updates. UnitGroup executes stored agent orders,
		 * performs vision/raycast work, and assigns the next orders before PhysicsSystem advances.
		 */
		void tick();

		/**
		 * @brief Accepts completed generation work, runs frame callbacks, and draws resident meshes.
		 *
		 * Body components update first; ChunkMeshGenerationSystem submits changed requests
		 * and accepts at most one valid generated mesh. The method then runs chunk frame callbacks, draws chunks,
		 * and conditionally draws debug overlays and the placement preview.
		 */
		void frame();

		/**
		 * @brief Resets world state, initializes asynchronous chunk generation, and creates the base scenario.
		 * Clears old chunks and bodies, resets the owning subsystems, then calls
		 * `startBaseScenario()` to repopulate point and body data. The mesh-generation
		 * subsystem advances its token so results from an earlier initialization are stale.
		 */
		void init();

		/**
		 * @brief Stops chunk generation, invalidates outstanding results, and releases world data.
		 * The mesh-generation subsystem joins its worker and clears requests before point and body pools are released.
		 * Call before destroying the graphics context so owned chunk meshes can
		 * release their GPU resources safely.
		 */
		void shutdown();

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
