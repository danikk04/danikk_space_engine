#pragma once

#include <danikk_space_engine/default.h>
#include <danikk_framework/dynamic_array.h>
#include <glm/glm.hpp>

namespace danikk_space_engine
{
	struct PlanetTerrainEdit
	{
		glm::dvec3 center = glm::dvec3(0.0);
		double radius = 0.0;
		bool add_material = false;
	};

	// Returns positive density inside terrain, zero on any terrain boundary, and negative density in air.
	double samplePlanetScalarField(
		const glm::dvec3& position_relative_to_planet_center,
		double radius,
		uint64 noise_seed,
		const DynamicArray<PlanetTerrainEdit>& edits);
}
