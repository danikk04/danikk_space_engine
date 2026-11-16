#pragma once

namespace danikk_space_engine
{

	/**
	 * @brief Represents game object data and behavior.
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
	 * @brief Identifies a terrain face by its owning chunk and face index.
	 */
	struct FacePtr;

	/**
	 * @brief Represents generated chunk data data and behavior.
	 */
	struct GeneratedChunkData;

	/**
	 * @brief Associates component data and callbacks with a pair of world points.
	 */
	struct GraphComponentSlot;

	/**
	 * @brief Associates component data and callbacks with a world point.
	 */
	struct PointComponentSlot;

	/**
	 * @brief Stores the nearest face hit and geometric data returned by a raycast.
	 */
	struct RaycastHit;

	/**
	 * @brief Represents radiation sample data and behavior.
	 */
	struct RadiationSample;

	/**
	 * @brief Represents temperature sample data and behavior.
	 */
	struct TemperatureSample;

	/**
	 * @brief Associates component data and callbacks with a world body.
	 */
	struct WorldBodyComponentSlot;
}
