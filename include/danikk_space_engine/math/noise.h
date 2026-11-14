#pragma once

#include <cstdint>

#include <danikk_space_engine/default.h>

namespace danikk_space_engine::math
{
	double latticeNoise3D(std::int64_t x, std::int64_t y, std::int64_t z);
	double valueNoise3D(const glm::dvec3& position);
	double fractalNoise3D(
		const glm::dvec3& position,
		uint8 octave_count = 3,
		double persistence = 0.5);
}
