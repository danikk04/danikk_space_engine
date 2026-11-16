#pragma once

#include <cstdint>

#include <danikk_space_engine/default.h>

namespace danikk_space_engine::math
{

	/**
	 * @brief Hashes an integer lattice coordinate into a deterministic value in [-1, 1].
	 * @param x Integer lattice coordinate on the X axis.
	 * @param y Integer lattice coordinate on the Y axis.
	 * @param z Integer lattice coordinate on the Z axis.
	 * Coordinates are folded into 32-bit unsigned arithmetic before hashing, giving
	 * the lattice pattern a period of 2^32 along each axis rather than an unbounded period.
	 */
	double latticeNoise3D(std::int64_t x, std::int64_t y, std::int64_t z);

	/**
	 * @brief Interpolates the eight surrounding lattice values to produce smooth 3D value noise.
	 * @param position Sample coordinate in noise space; integer floors select the lattice cell.
	 * @return Interpolated value in the range of the lattice samples, nominally [-1, 1].
	 * Quintic fade weights are applied independently on each axis before trilinear
	 * interpolation. Since each fractional cell coordinate lies in [0, 1], the
	 * result remains within the range of the eight sampled corner values.
	 */
	double valueNoise3D(const glm::dvec3& position);

	/**
	 * @brief Combines progressively finer value-noise samples and normalizes by total amplitude.
	 * @param position Base coordinate in noise space.
	 * @param octave_count Number of samples to accumulate; zero returns 0.
	 * @param persistence Multiplier applied to amplitude after each octave.
	 * Each octave doubles the sample coordinate and scales amplitude by persistence.
	 * The result is divided by the sum of amplitudes when that sum is positive; if
	 * it is zero or negative, the function returns zero.
	 * @return Normalized fractal value noise.
	 */
	double fractalNoise3D(
		const glm::dvec3& position,
		uint8 octave_count = 3,
		double persistence = 0.5);
}
