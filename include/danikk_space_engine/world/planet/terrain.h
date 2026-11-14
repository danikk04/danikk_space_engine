#pragma once

#include <danikk_space_engine/default.h>
#include <glm/glm.hpp>

namespace danikk_space_engine
{
	// Returns positive values inside the procedural planet, zero on its surface, and negative values outside.
	double samplePlanetScalarField(
		const glm::dvec3& position_relative_to_planet_center,
		double radius,
		uint64 noise_seed);
}
