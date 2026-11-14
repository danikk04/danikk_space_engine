#include <danikk_space_engine/world/planet.h>

namespace danikk_space_engine
{
	double samplePlanetScalarField(const glm::dvec3& position_relative_to_planet_center, double radius)
	{
		return radius - glm::length(position_relative_to_planet_center);
	}
}
