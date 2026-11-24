#pragma once

namespace danikk_space_engine
{

	/**
	 * @brief Provides the legacy object transform, shape, and collision interface.
	 */
	class GameObject;

	/**
	 * @brief Owns player input, selection, tools, and the currently controlled point.
	 */
	class ControllerSystem;

	/**
	 * @brief Coordinates world state, simulation updates, physics, and geometry queries.
	 */
	class World;
	struct WorldSystemContainer;
	class TelemetrySystem;
	class PointData;
	class WorldBodySystem;
	class BuildObjectSystem;
	class BuildPartSystem;
	class MeshGenerationSystem;
	class ComponentSystem;
	class RaycastSystem;
	class FactionSystem;
	class UnitSystem;


	/** @brief Runtime configuration and streaming state for one planet. */
	struct PlanetData;

	/** @brief Owns planet state and provides terrain queries and streaming. */
	class PlanetSystem;
	class PlanetRenderSystem;

	/** @brief Owns world-save paths and persistent data serialization. */
	class SaveSystem;

	/** @brief Concrete item metadata referenced by item-category callbacks. */
	struct ItemDefinition;

	/**
	 * @brief Computes environmental fields at a world position.
	 */
	class EnvironmentSystem;

	/**
	 * @brief Integrates world body motion and resolves physical collisions.
	 */
	class PhysicsSystem;

	/** @brief Owns people, their local positions, inventory, health, and lifecycle. */
	class HumanSystem;

	/** @brief Owns active camera state, movement, and projection. */
	class CameraSystem;

	/**
	 * @brief Stores a human character’s state, orientation, pose, and appearance.
	 */
	struct HumanState;

	/**
	 * @brief Holds worker-generated mesh data before main-thread resource creation.
	 */
	struct MeshGenerationJob;

	/**
	 * @brief Associates component data and callbacks with a world point.
	 */
	struct PointComponentSlot;

	/** @brief Stores a point's local position in its owning body's frame. */
	struct Point;

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

}
