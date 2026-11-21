#pragma once

#include <danikk_space_engine/fwd.h>
#include <danikk_space_engine/id.h>
#include <danikk_space_engine/geometry/build_part.h>

namespace danikk_space_engine
{

	/**
	 * @brief Runs the fixed-step physics pipeline for the world.
	 *
	 * The object is owned by World and keeps no persistent simulation state. Each
	 * tick executes motion integration, removal inside planetary cores, planet
	 * contacts, rigid-body contacts, and human capsule separation in that order.
	 * Later contact stages do not cause an earlier stage to run again during the
	 * same tick.
	 */
	class PhysicsSystem
	{
		/** @brief Owning World whose bodies and contacts this system simulates. */
		World& world;

		/**
		 * @brief Applies gravity and integrates position and orientation of loaded, non-planet bodies.
		 *
		 * Uses the engine's target frame delay. Bodies without an point-owned entities,
		 * planet bodies, and the body currently held by the Drag tool are skipped.
		 * Gravity is sampled from EnvironmentSystem before linear position is
		 * advanced; angular velocity is integrated as an axis-angle rotation.
		 */
		void integrateMotion();

		/**
		 * @brief Destroys loaded non-planet bodies and point entities inside a planet's 5 m core radius.
		 *
		 * Planet centers are snapshotted first. IDs are collected before pool
		 * destruction to avoid invalidating iteration. The camera point and planet
		 * bodies are exempt; point components in unpoint-owned entities are not examined.
		 */
		void destroyEntitiesInsidePlanetCore();

		/**
		 * @brief Resolves contacts between loaded non-planet bodies and planet scalar fields.
		 *
		 * Mesh-bearing bodies use their point-owned entities points to build grouped terrain
		 * contacts from scalar-field density and gradient samples. Bodies without
		 * surface triangles may use collision_radius as a spherical fallback.
		 * Corrections and impulses affect the moving body; planets are immovable.
		 */
		void resolvePlanetCollisions();

		/**
		 * @brief Resolves pairwise contacts between loaded non-planet mesh bodies.
		 *
		 * Convex BuildPart proxies are built once per call and tested in four solver
		 * passes. Body and part bounding spheres reject distant pairs before SAT;
		 * each BuildPart is treated as convex. A dragged body is assigned zero inverse
		 * mass and inertia for this solver.
		 */
		void resolveBodyCollisions();

		/**
		 * @brief Separates overlapping human capsules and restores ground clearance afterward.
		 *
		 * Uses up to four positional-correction passes, visiting each candidate part pair
		 * once per pass. Human dimensions and up vectors define capsule axes.
		 * Corrections are projected onto each person's
		 * local tangent plane. A short raycast may lift a person after separation,
		 * but this stage does not apply collision impulses or resolve against terrain.
		 */
		void resolveHumanCollisions();

	public:
		/** @brief Logs initialization of the physics subsystem. */
		void init();
		/** @brief Logs shutdown of the physics subsystem. */
		void shutdown();
		/** @brief Binds physics to its owning World. */
		explicit PhysicsSystem(World& world) : world(world) {}

		/**
		 * @brief Advances physics by one update step using the documented stage order.
		 */
		void tick();

		/**
		 * @brief Checks whether a proposed BuildPart penetrates another part on the same body.
		 * @param body_id Existing body receiving the part.
		 * @param shape Primitive geometry of the proposed part.
		 * @param world_position Proposed part center in world coordinates.
		 * @param body_local_rotation Proposed orientation in the body's local frame.
		 * @param size Per-axis dimensions of the proposed part.
		 * @param penetration_tolerance Overlap no greater than this is treated as face contact.
		 * @return False when the candidate intersects any existing BuildPart beyond the tolerance.
		 */
		bool isBuildPartPlacementClear(
			WorldBodyId body_id, BuildPartShape shape,
			const glm::dvec3& world_position, const mat4& body_local_rotation,
			const vec3& size, double penetration_tolerance = 0.001) const;
	};
}
