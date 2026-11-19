#pragma once

#include <danikk_framework/glm.h>
#include <danikk_space_engine/fwd.h>
#include <danikk_space_engine/id.h>
#include <danikk_space_engine/material.h>

namespace danikk_space_engine
{
	struct PlanetTerrainSnapshot;

	/**
	 * @brief Samples a planet's scalar field and terrain material within its owning World.
	 *
	 * Live-world queries resolve component IDs through the World reference stored at construction.
	 * Snapshot overloads are static so worker threads can sample immutable terrain data without
	 * accessing WorldData concurrently.
	 */
	class PlanetTerrainSystem
	{
		/** @brief Owning World whose planet components provide live terrain data. */
		World& world;

	public:
		/** @brief Binds terrain sampling to its owning World. */
		explicit PlanetTerrainSystem(World& world) : world(world) {}

		/**
		 * @brief Samples signed density from the field owned by a planet component.
		 * @param planet_component_id WorldBody component slot identifying the planet field.
		 * @param position Position in coordinates relative to the planet center.
		 * @return Positive density inside solid terrain, negative density in air, and zero on the surface.
		 */
		double getPlanetDensity(WorldBodyComponentSlotId planet_component_id,
			const glm::dvec3& position) const;
		/** @brief Samples signed density from a worker-safe immutable planet-field snapshot. */
		static double getPlanetDensity(const PlanetTerrainSnapshot& snapshot, const glm::dvec3& position);

		/** @brief Selects the material at a point relative to the identified planet center. */
		MaterialId getPlanetMaterial(WorldBodyComponentSlotId planet_component_id,
			const glm::dvec3& position) const;
		/** @brief Selects material from an immutable worker-safe planet-field snapshot. */
		static MaterialId getPlanetMaterial(const PlanetTerrainSnapshot& snapshot, const glm::dvec3& position);
	};
}
