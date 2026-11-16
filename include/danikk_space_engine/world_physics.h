#pragma once

#include <danikk_space_engine/fwd.h>

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
	class WorldPhysics
	{

		/**
		 * @brief Applies gravity and integrates position and orientation of loaded, non-planet bodies.
		 *
		 * Uses the engine's target frame delay. Bodies without an active chunk,
		 * planet bodies, and the body currently held by the Drag tool are skipped.
		 * Gravity is sampled from WorldEnvironment before linear position is
		 * advanced; angular velocity is integrated as an axis-angle rotation.
		 * @param world World whose bodies and environment are updated.
		 */
		void integrateMotion(World& world);

		/**
		 * @brief Destroys loaded non-planet bodies and point entities inside a planet's 5 m core radius.
		 *
		 * Planet centers are snapshotted first. IDs are collected before pool
		 * destruction to avoid invalidating iteration. The camera point and planet
		 * bodies are exempt; point components in unloaded chunks are not examined.
		 * @param world World whose resource pools are modified.
		 */
		void destroyEntitiesInsidePlanetCore(World& world);

		/**
		 * @brief Resolves contacts between loaded non-planet bodies and planet scalar fields.
		 *
		 * Mesh-bearing bodies use their active chunk points to build grouped terrain
		 * contacts from scalar-field density and gradient samples. Bodies without
		 * surface triangles may use collision_radius as a spherical fallback.
		 * Corrections and impulses affect the moving body; planets are immovable.
		 * @param world World containing planet fields and moving bodies.
		 */
		void resolvePlanetCollisions(World& world);

		/**
		 * @brief Resolves pairwise contacts between loaded non-planet mesh bodies.
		 *
		 * Convex chunk proxies are built once per call and tested in four solver
		 * passes. Body and chunk bounding spheres reject distant pairs before SAT;
		 * each chunk is treated as convex. A dragged body is assigned zero inverse
		 * mass and inertia for this solver.
		 * @param world World whose body positions and velocities may be corrected.
		 */
		void resolveBodyCollisions(World& world);

		/**
		 * @brief Separates overlapping human capsules and restores ground clearance afterward.
		 *
		 * Uses four pairwise positional-correction passes. Human dimensions and up
		 * vectors define capsule axes; corrections are projected onto each person's
		 * local tangent plane. A short raycast may lift a person after separation,
		 * but this stage does not apply collision impulses or resolve against terrain.
		 * @param world World whose human point positions are adjusted.
		 */
		void resolveHumanCollisions(World& world);

	public:

		/**
		 * @brief Advances physics by one update step using the documented stage order.
		 * @param world World to simulate.
		 */
		void tick(World& world);
	};
}
