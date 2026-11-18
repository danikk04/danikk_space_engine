#pragma once

namespace danikk_space_engine
{

	/**
	 * @brief Provides the legacy object transform, shape, and collision interface.
	 */
	class GameObject;

	/**
	 * @brief Processes player input, face selection, and active tool actions.
	 */
	class LocalPlayerControllerComponent;

	/**
	 * @brief Coordinates world state, simulation updates, physics, and geometry queries.
	 */
	class World;

	/**
	 * @brief Computes environmental fields at a world position.
	 */
	class WorldEnvironment;

	/**
	 * @brief Integrates world body motion and resolves physical collisions.
	 */
	class WorldPhysics;

	/**
	 * @brief Stores a human character’s state, orientation, pose, and appearance.
	 */
	struct HumanComponent;

	/**
	 * @brief Holds worker-generated chunk vertices, indices, and material data before world resources are created.
	 */
	struct GeneratedChunkMesh;

	/**
	 * @brief Associates component data and callbacks with a world point.
	 */
	struct PointComponentSlot;

	/**
	 * @brief Stores the nearest face hit and geometric data returned by a raycast.
	 */
	struct RaycastHit;

	/**
	 * @brief Returns a radiation dose rate together with a flag indicating whether the field is available.
	 */
	struct RadiationSample;

	/**
	 * @brief Returns a temperature in kelvin together with a flag indicating whether the field is available.
	 */
	struct TemperatureSample;

	/**
	 * @brief Associates component data and callbacks with a world body.
	 */
	struct WorldBodyComponentSlot;
}
