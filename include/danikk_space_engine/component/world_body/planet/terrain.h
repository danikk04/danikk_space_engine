#pragma once

#include <danikk_space_engine/default.h>
#include <danikk_framework/dynamic_array.h>
#include <danikk_space_engine/fwd.h>
#include <danikk_space_engine/id.h>
#include <glm/glm.hpp>

namespace danikk_space_engine
{

	/**
	 * @brief Stores a persistent spherical terrain edit with position, radius, and add/remove operation.
	 */
	struct PlanetTerrainEdit
	{
		/** @brief Center of the spherical terrain stamp in coordinates relative to the planet center. */
		glm::dvec3 center = glm::dvec3(0.0);
		/** @brief Stamp radius in world units. */
		double radius = 0.0;
		/** @brief True adds solid terrain; false subtracts a spherical region from the field. */
		bool adds_solid_terrain = false;
	};

	/**
	 * @brief Finds the first density sign change on a finite planet-local ray.
	 * @param world World containing the planet component and its edits.
	 * @param planet_component_id ID of the planet component slot.
	 * @param origin Ray origin relative to the planet center.
	 * @param direction Ray direction in planet-local coordinates; normalized internally.
	 * @param max_distance Maximum search distance in metres.
	 * @param distance Receives the distance from the origin to the detected surface.
	 * @param normal Receives the outward unit normal in planet-local coordinates.
	 * @return True when a transition is found and its normal is valid.
	 * Uses half-metre samples and refines a detected transition by bisection. Features
	 * thinner than the sampling step can be missed when both boundaries lie between samples.
	 */
	bool raycastPlanetScalarField(const World& world, WorldBodyComponentSlotId planet_component_id,
		const glm::dvec3& origin, const glm::dvec3& direction, double max_distance,
		double& distance, glm::dvec3& normal);

}
