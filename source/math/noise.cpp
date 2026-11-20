#include <cmath>
#include <cstdint>
#include <limits>

#include <danikk_space_engine/math/interpolation.h>
#include <danikk_space_engine/math/noise.h>

namespace danikk_space_engine::math
{
	namespace
	{
		/** @brief Hashes a signed lattice coordinate to a stable 32-bit value. */
		uint32_t hashLatticeCoordinate(std::int64_t x, std::int64_t y, std::int64_t z)
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
			return hash;
		}

		/** @brief Computes one attenuated simplex-corner contribution. */
		double simplexCornerContribution(const glm::dvec3& offset,
			std::int64_t x, std::int64_t y, std::int64_t z)
		{
			constexpr glm::dvec3 gradients[12] = {
				{1.0, 1.0, 0.0}, {-1.0, 1.0, 0.0}, {1.0, -1.0, 0.0}, {-1.0, -1.0, 0.0},
				{1.0, 0.0, 1.0}, {-1.0, 0.0, 1.0}, {1.0, 0.0, -1.0}, {-1.0, 0.0, -1.0},
				{0.0, 1.0, 1.0}, {0.0, -1.0, 1.0}, {0.0, 1.0, -1.0}, {0.0, -1.0, -1.0}};
			constexpr double attenuation_radius_squared = 0.6;
			double attenuation = attenuation_radius_squared - glm::dot(offset, offset);
			if(attenuation <= 0.0) return 0.0;
			const size_t gradient_index = hashLatticeCoordinate(x, y, z) % 12;
			attenuation *= attenuation;
			return attenuation * attenuation * glm::dot(gradients[gradient_index], offset);
		}
	}

	/**
	 * @brief Mixes the low 32 bits of lattice coordinates into a deterministic value in [-1, 1].
	 * @param x Lattice X coordinate.
	 * @param y Lattice Y coordinate.
	 * @param z Lattice Z coordinate.
	 * Coordinate products are combined with XOR and passed through a 32-bit avalanche
	 * hash, then mapped from the unsigned integer range to approximately [-1, 1].
	 */
	double latticeNoise3D(std::int64_t x, std::int64_t y, std::int64_t z)
	{
		const uint32_t hash = hashLatticeCoordinate(x, y, z);
		return static_cast<double>(hash) / std::numeric_limits<uint32_t>::max() * 2.0 - 1.0;
	}

	/**
	 * @brief Computes 3D value noise by quintic-weighted interpolation of eight lattice samples.
	 * @param position Position in noise space.
	 * @return Smooth interpolation of the cell's corner values.
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
	 * @brief Evaluates 3D simplex gradient noise using the four corners of a tetrahedral cell.
	 * @param position Position in noise space.
	 * @return Smooth deterministic simplex value, scaled to approximately [-1, 1].
	 */
	double simplexNoise3D(const glm::dvec3& position)
	{
		constexpr double skew_factor = 1.0 / 3.0;
		constexpr double unskew_factor = 1.0 / 6.0;
		const double skew = (position.x + position.y + position.z) * skew_factor;
		const std::int64_t cell_x = static_cast<std::int64_t>(std::floor(position.x + skew));
		const std::int64_t cell_y = static_cast<std::int64_t>(std::floor(position.y + skew));
		const std::int64_t cell_z = static_cast<std::int64_t>(std::floor(position.z + skew));
		const double unskew = static_cast<double>(cell_x + cell_y + cell_z) * unskew_factor;
		const glm::dvec3 cell_origin(
			static_cast<double>(cell_x) - unskew,
			static_cast<double>(cell_y) - unskew,
			static_cast<double>(cell_z) - unskew);
		const glm::dvec3 first_offset = position - cell_origin;

		std::int64_t first_x = 0;
		std::int64_t first_y = 0;
		std::int64_t first_z = 0;
		std::int64_t second_x = 0;
		std::int64_t second_y = 0;
		std::int64_t second_z = 0;
		if(first_offset.x >= first_offset.y)
		{
			if(first_offset.y >= first_offset.z)
			{
				first_x = 1; second_x = 1; second_y = 1;
			}
			else if(first_offset.x >= first_offset.z)
			{
				first_x = 1; second_x = 1; second_z = 1;
			}
			else
			{
				first_z = 1; second_x = 1; second_z = 1;
			}
		}
		else
		{
			if(first_offset.y < first_offset.z)
			{
				first_z = 1; second_y = 1; second_z = 1;
			}
			else if(first_offset.x < first_offset.z)
			{
				first_y = 1; second_y = 1; second_z = 1;
			}
			else
			{
				first_y = 1; second_x = 1; second_y = 1;
			}
		}

		const glm::dvec3 second_offset = first_offset
			- glm::dvec3(first_x, first_y, first_z) + glm::dvec3(unskew_factor);
		const glm::dvec3 third_offset = first_offset
			- glm::dvec3(second_x, second_y, second_z) + glm::dvec3(2.0 * unskew_factor);
		const glm::dvec3 fourth_offset = first_offset - glm::dvec3(1.0)
			+ glm::dvec3(3.0 * unskew_factor);

		const double value = simplexCornerContribution(first_offset, cell_x, cell_y, cell_z)
			+ simplexCornerContribution(second_offset, cell_x + first_x, cell_y + first_y, cell_z + first_z)
			+ simplexCornerContribution(third_offset, cell_x + second_x, cell_y + second_y, cell_z + second_z)
			+ simplexCornerContribution(fourth_offset, cell_x + 1, cell_y + 1, cell_z + 1);
		return value * 32.0;
	}

	/**
	 * @brief Accumulates finer value-noise octaves with geometric amplitude decay.
	 * @param position Base sample position.
	 * @param octave_count Number of octaves to sum.
	 * @param persistence Amplitude multiplier between adjacent octaves.
	 * @return Weighted sum divided by the accumulated amplitude, or zero when no
	 *         positive amplitude was accumulated.
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

	/**
	 * @brief Accumulates normalized simplex octaves with geometric frequency and amplitude growth.
	 * @param position Base sample position.
	 * @param octave_count Number of octaves to sum.
	 * @param persistence Amplitude multiplier per octave.
	 * @param lacunarity Frequency multiplier per octave.
	 * @return Weighted simplex sum normalized by total amplitude.
	 */
	double fractalSimplexNoise3D(const glm::dvec3& position, uint8 octave_count,
		double persistence, double lacunarity)
	{
		double result = 0.0;
		double amplitude = 1.0;
		double amplitude_sum = 0.0;
		glm::dvec3 octave_position = position;
		for(uint8 octave = 0; octave < octave_count; octave++)
		{
			result += simplexNoise3D(octave_position) * amplitude;
			amplitude_sum += amplitude;
			amplitude *= persistence;
			octave_position *= lacunarity;
		}
		return amplitude_sum > 0.0 ? result / amplitude_sum : 0.0;
	}
}
