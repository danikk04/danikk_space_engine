#pragma once

#include <danikk_space_engine/default.h>
#include <glm/glm.hpp>

namespace danikk_space_engine
{
	inline constexpr double default_planet_radius = 150000000.0;

	// Returns positive values inside the spherical planet, zero on its surface, and negative values outside.
	double samplePlanetScalarField(
		const glm::dvec3& position_relative_to_planet_center,
		double radius = default_planet_radius);
}
