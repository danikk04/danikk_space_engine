#include <cmath>
#include <cstdint>
#include <limits>

#include <danikk_space_engine/math/interpolation.h>
#include <danikk_space_engine/math/noise.h>

namespace danikk_space_engine::math
{

	/**
	 * @brief Hashes integer lattice coordinates into a deterministic scalar noise value.
	 */
	double latticeNoise3D(std::int64_t x, std::int64_t y, std::int64_t z)
	{
		uint32_t hash = static_cast<uint32_t>(x) * 0x8DA6B343u;
		hash ^= static_cast<uint32_t>(y) * 0xD8163841u;
		hash ^= static_cast<uint32_t>(z) * 0xCB1AB31Fu;
		hash += 0x9E3779B9u;
		hash ^= hash >> 16;
		hash *= 0x7FEB352Du;
		hash ^= hash >> 15;
		hash *= 0x846CA68Bu;
		hash ^= hash >> 16;
		return static_cast<double>(hash) / std::numeric_limits<uint32_t>::max() * 2.0 - 1.0;
	}

	/**
	 * @brief Interpolates deterministic lattice values to produce smooth three-dimensional value noise.
	 */
	double valueNoise3D(const glm::dvec3& position)
	{
		const std::int64_t x0 = static_cast<std::int64_t>(std::floor(position.x));
		const std::int64_t y0 = static_cast<std::int64_t>(std::floor(position.y));
		const std::int64_t z0 = static_cast<std::int64_t>(std::floor(position.z));
		const double x = quinticFade(position.x - static_cast<double>(x0));
		const double y = quinticFade(position.y - static_cast<double>(y0));
		const double z = quinticFade(position.z - static_cast<double>(z0));

		const double z00 = linearInterpolate(
			linearInterpolate(latticeNoise3D(x0, y0, z0), latticeNoise3D(x0 + 1, y0, z0), x),
			linearInterpolate(latticeNoise3D(x0, y0 + 1, z0), latticeNoise3D(x0 + 1, y0 + 1, z0), x),
			y);
		const double z01 = linearInterpolate(
			linearInterpolate(latticeNoise3D(x0, y0, z0 + 1), latticeNoise3D(x0 + 1, y0, z0 + 1), x),
			linearInterpolate(latticeNoise3D(x0, y0 + 1, z0 + 1), latticeNoise3D(x0 + 1, y0 + 1, z0 + 1), x),
			y);
		return linearInterpolate(z00, z01, z);
	}

	/**
	 * @brief Sums scaled value-noise octaves using the requested octave count and persistence.
	 */
	double fractalNoise3D(const glm::dvec3& position, uint8 octave_count, double persistence)
	{
		double result = 0.0;
		double amplitude = 1.0;
		double amplitude_sum = 0.0;
		glm::dvec3 octave_position = position;
		for(uint8 octave = 0; octave < octave_count; octave++)
		{
			result += valueNoise3D(octave_position) * amplitude;
			amplitude_sum += amplitude;
			amplitude *= persistence;
			octave_position *= 2.0;
		}
		return amplitude_sum > 0.0 ? result / amplitude_sum : 0.0;
	}
}
