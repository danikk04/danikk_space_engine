#pragma once

#include <danikk_space_engine/default.h>
#include <danikk_framework/dynamic_array.h>
#include <danikk_space_engine/material.h>
#include <glm/glm.hpp>

namespace danikk_space_engine
{

	/**
	 * @brief Stores a persistent spherical terrain edit with position, radius, and add/remove operation.
	 */
	struct PlanetTerrainEdit
	{
		glm::dvec3 center = glm::dvec3(0.0);
		double radius = 0.0;
		// Selects additive solid terrain (true) or a subtractive spherical cut (false).
		bool adds_solid_terrain = false;
	};

	// Returns positive density inside terrain, zero on any terrain boundary, and negative density in air.

	/**
	 * @brief Computes a sample for planet scalar field.
	 */
	double samplePlanetScalarField(
		const glm::dvec3& position_relative_to_planet_center,
		double radius,
		uint64 noise_seed,
		const DynamicArray<PlanetTerrainEdit>& edits,
		double core_radius);

	/**
	 * @brief Returns planet surface material.
	 */
	MaterialId getPlanetSurfaceMaterial(
		const glm::dvec3& position_relative_to_planet_center,
		double radius,
		uint64 noise_seed);
}
