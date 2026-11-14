#pragma once

#include <danikk_space_engine/default.h>
#include <glm/glm.hpp>

namespace danikk_space_engine
{
	inline constexpr double default_planet_radius = 500.0;
	inline constexpr double planet_chunk_stream_radius = 100.0;

	// Returns positive values inside the procedural planet, zero on its surface, and negative values outside.
	double samplePlanetScalarField(
		const glm::dvec3& position_relative_to_planet_center,
		double radius = default_planet_radius);

	class World;
	void initializePlanetStreaming(World& world, uint32 world_body_id);
	void updatePlanetStreaming(World& world);
}
