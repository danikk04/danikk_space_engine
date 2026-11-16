#pragma once

#include <cstdint>

#include <danikk_space_engine/default.h>

namespace danikk_space_engine::math
{

	/**
	 * @brief Hashes integer lattice coordinates into a deterministic scalar noise value.
	 */
	double latticeNoise3D(std::int64_t x, std::int64_t y, std::int64_t z);

	/**
	 * @brief Interpolates deterministic lattice values to produce smooth three-dimensional value noise.
	 */
	double valueNoise3D(const glm::dvec3& position);

	/**
	 * @brief Sums scaled value-noise octaves using the requested octave count and persistence.
	 */
	double fractalNoise3D(
		const glm::dvec3& position,
		uint8 octave_count = 3,
		double persistence = 0.5);
}
