#pragma once

#include <danikk_space_engine/default.h>
#include <danikk_space_engine/fwd.h>
#include <danikk_space_engine/id.h>

#include <memory>

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
		/** @brief Constructs subsystems bound to this World instance. */
		World();
		~World();
		World(const World&) = delete;
		World& operator=(const World&) = delete;

		/** @brief Returns the telemetry system owned by this world. */
		TelemetrySystem& getTelemetrySystem();
		const TelemetrySystem& getTelemetrySystem() const;
		/** @brief Returns the point and WorldBody data containers. */
		PointData& getPointData();
		const PointData& getPointData() const;
		WorldBodyData& getWorldBodyData();
		const WorldBodyData& getWorldBodyData() const;
		/** @brief Returns the asynchronous mesh generation system. */
		MeshGenerationSystem& getMeshGenerationSystem();
		const MeshGenerationSystem& getMeshGenerationSystem() const;
		/** @brief Returns the environment simulation system. */
		EnvironmentSystem& getEnvironmentSystem();
		const EnvironmentSystem& getEnvironmentSystem() const;
		/** @brief Returns the physics simulation system. */
		PhysicsSystem& getPhysicsSystem();
		const PhysicsSystem& getPhysicsSystem() const;
		/** @brief Returns the local player controller system. */
		ControllerSystem& getControllerSystem();
		const ControllerSystem& getControllerSystem() const;
		/** @brief Returns the camera system. */
		CameraSystem& getCameraSystem();
		const CameraSystem& getCameraSystem() const;
		/** @brief Returns the component dispatch system. */
		ComponentSystem& getComponentSystem();
		const ComponentSystem& getComponentSystem() const;
		/** @brief Returns the planet and terrain system. */
		PlanetSystem& getPlanetSystem();
		const PlanetSystem& getPlanetSystem() const;
		/** @brief Returns the raycast system. */
		RaycastSystem& getRaycastSystem();
		const RaycastSystem& getRaycastSystem() const;
		/** @brief Returns the save system. */
		SaveSystem& getSaveSystem();
		const SaveSystem& getSaveSystem() const;
		/** @brief Returns the faction system. */
		FactionSystem& getFactionSystem();
		const FactionSystem& getFactionSystem() const;
		/** @brief Returns the unit system. */
		UnitSystem& getUnitSystem();
		const UnitSystem& getUnitSystem() const;
		/** @brief Returns the human entity system. */
		HumanSystem& getHumanSystem();
		const HumanSystem& getHumanSystem() const;

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
		/** @brief Mirrors planet-owned transforms into temporary WorldBody frames used by legacy entity ownership. */
		void syncPlanetWorldBodyFrames();

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

	private:
		std::unique_ptr<WorldSystemContainer> system_container;
	};

}
