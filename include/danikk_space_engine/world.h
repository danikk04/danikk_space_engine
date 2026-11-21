#pragma once

#include <danikk_space_engine/default.h>
#include <danikk_space_engine/systems/environment/environment.h>
#include <danikk_space_engine/systems/physics/physics.h>

#include <danikk_space_engine/point.h>
#include <danikk_space_engine/systems/camera/camera.h>
#include <danikk_space_engine/systems/controller/controller.h>
#include <danikk_space_engine/systems/component/component.h>
#include <danikk_space_engine/world_body.h>
#include <danikk_space_engine/systems/mesh_generation/mesh_generation.h>
#include <danikk_space_engine/systems/planet/system.h>
#include <danikk_space_engine/systems/raycast/raycast.h>
#include <danikk_space_engine/systems/save/system.h>
#include <danikk_space_engine/systems/faction/faction.h>
#include <danikk_space_engine/systems/unit/unit.h>
#include <danikk_space_engine/systems/human/system.h>

namespace danikk_space_engine
{

	/**
	 * @brief Coordinates world state, simulation updates, physics, and geometry queries.
	 *
	 * PointData and WorldBodyData own resource lifetimes. World coordinates body-local point ownership,
	 * component callbacks, deferred
	 * destructive point cleanup until callback traversal finishes, then advances PhysicsSystem.
	 * Mesh generation runs on a worker; completed data is validated
	 * and accepted by the main thread before mesh creation and drawing.
	 */
	class World
	{
	public:
		/** @brief Owns point resources and their component slots. */
		PointData point_data;
		/** @brief Owns WorldBody resources and their component slots. */
		WorldBodyData world_body_data;
		/** @brief Owns asynchronous terrain-node and body-detail mesh requests. */
		MeshGenerationSystem mesh_generation;
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
		/** @brief Owns planet data, terrain queries, edits, and terrain-node streaming. */
		PlanetSystem planets;
		/** @brief Performs terrain and BuildPart ray queries. */
		RaycastSystem raycasts;
		/** @brief Owns world-save paths, formats, and persistent data serialization. */
		SaveSystem saves;
		/** @brief Owns this world's faction instances and diplomatic relations. */
		FactionSystem factions;
		/** @brief Owns per-faction unit groups, tactical memory, and orders. */
		UnitSystem unit_system;
		/** @brief Owns human entities, their positions, health, and inventory. */
		HumanSystem humans;

		/** @brief Constructs subsystems bound to this World instance. */
		World();
		World(const World&) = delete;
		World& operator=(const World&) = delete;

		/**
		 * @brief Executes current unit orders, updates UnitGroup decisions, then runs world physics.
		 *
		 * Component ID lists are snapshotted where callbacks may mutate ownership lists.
		 * Deferred point deletion is flushed after component and controller updates. UnitGroup executes stored agent orders,
		 * performs vision/raycast work, and assigns the next orders before PhysicsSystem advances.
		 */
		void tick();

		/**
		 * @brief Accepts completed generation work, runs frame callbacks, and draws resident meshes.
		 *
		 * Body components update first; mesh generation submits changed requests
		 * and accepts at most one valid generated mesh. The method then runs point frame callbacks and draws meshes,
		 * and conditionally draws debug overlays and the placement preview.
		 */
		void frame();
		/** @brief Draws planet-tree and WorldBody-owned detail meshes. */
		void renderWorldMeshes();

		/**
		 * @brief Resets world state, initializes asynchronous mesh generation, and creates the base scenario.
		 * Clears old bodies and resets the owning subsystems, then calls
		 * `startBaseScenario()` to repopulate point and body data. The mesh-generation
		 * subsystem advances its token so results from an earlier initialization are stale.
		 */
		void init();

		/**
		 * @brief Stops mesh generation, invalidates outstanding results, and releases world data.
		 * The mesh-generation subsystem joins its worker and clears requests before point and body pools are released.
		 * Call before destroying the graphics context so owned meshes can
		 * release their GPU resources safely.
		 */
		void shutdown();

		/** @brief Transforms a point's canonical body-local position into world coordinates. */
		glm::dvec3 getPointWorldPosition(PointId point_id) const;
		/** @brief Builds a camera-relative transform at a point in its WorldBody frame. */
		mat4 getPointWorldMatrix(PointId point_id) const;
		/** @brief Attaches a point-component slot directly to a body's local octree. */
		void attachPointComponentToBody(WorldBodyId body_id, PointId point_id,
			PointComponentSlotId slot_id, const glm::dvec3& body_local_position);
		/** @brief Attaches a point to a body's local frame and spatial tree. */
		void attachPointToBody(WorldBodyId body_id, PointId point_id,
			const glm::dvec3& body_local_position);
		/** @brief Moves a point within its owning body and updates the body octree. */
		void setPointBodyLocalPosition(PointId point_id, const glm::dvec3& body_local_position);
		/** @brief Removes a point from its body spatial index and destroys its components. */
		void destroyPoint(PointId point_id);
		/** @brief Destroys all body-owned points before releasing the body and planet records. */
		void destroyWorldBody(WorldBodyId body_id);

	};

}
