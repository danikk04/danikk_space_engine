#pragma once

#include <danikk_space_engine/fwd.h>

namespace danikk_space_engine
{

	/**
	 * @brief Integrates world body motion and resolves physical collisions.
	 */
	class WorldPhysics
	{

		/**
		 * @brief Integrates motion.
		 */
		void integrateMotion(World& world);

		/**
		 * @brief Removes and releases entities inside planet core.
		 */
		void destroyEntitiesInsidePlanetCore(World& world);

		/**
		 * @brief Resolves planet collisions.
		 */
		void resolvePlanetCollisions(World& world);

		/**
		 * @brief Resolves body collisions.
		 */
		void resolveBodyCollisions(World& world);

		/**
		 * @brief Resolves human collisions.
		 */
		void resolveHumanCollisions(World& world);

	public:

		/**
		 * @brief Performs the tick operation.
		 */
		void tick(World& world);
	};
}
