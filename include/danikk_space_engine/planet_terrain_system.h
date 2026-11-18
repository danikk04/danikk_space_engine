#pragma once

#include <danikk_framework/glm.h>
#include <danikk_space_engine/fwd.h>
#include <danikk_space_engine/id.h>
#include <danikk_space_engine/material.h>

namespace danikk_space_engine
{
	struct PlanetTerrainSnapshot;

	/**
	 * @brief Samples signed density from the field owned by a planet component.
	 * @param world World containing the component slot and its terrain edits.
	 * @param planet_component_id WorldBody component slot identifying the planet field.
	 * @param position Position in coordinates relative to the planet center.
	 * @return Positive density inside solid terrain, negative density in air, and zero on the surface.
	 */
	double getPlanetDensity(const World& world, WorldBodyComponentSlotId planet_component_id,
		const glm::dvec3& position);
	/** @brief Samples signed density from a worker-safe immutable planet-field snapshot. */
	double getPlanetDensity(const PlanetTerrainSnapshot& snapshot, const glm::dvec3& position);

	/**
	 * @brief Selects the material at a position using the identified planet's terrain settings.
	 * @param world World containing the planet component.
	 * @param planet_component_id WorldBody component slot identifying the planet field.
	 * @param position Position in coordinates relative to the planet center.
	 * @return Material selected for the sampled terrain location.
	 */
	MaterialId getPlanetMaterial(const World& world, WorldBodyComponentSlotId planet_component_id,
		const glm::dvec3& position);
	/** @brief Selects material from the same immutable snapshot used to generate a chunk mesh. */
	MaterialId getPlanetMaterial(const PlanetTerrainSnapshot& snapshot, const glm::dvec3& position);
}
