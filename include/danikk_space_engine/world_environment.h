#pragma once

#include <glm/glm.hpp>

namespace danikk_space_engine
{

	/**
	 * @brief Coordinates world state, simulation updates, physics, and geometry queries.
	 */
	class World;

	/**
	 * @brief Represents temperature sample data and behavior.
	 */
	struct TemperatureSample
	{
		double temperature_kelvin = 0.0;
		bool available = false;
	};

	/**
	 * @brief Represents radiation sample data and behavior.
	 */
	struct RadiationSample
	{
		double radiation_dose_rate_sv_per_second = 0.0;
		bool available = false;
	};

	/**
	 * @brief Computes environmental fields at a world position.
	 */
	class WorldEnvironment
	{
	public:

		/**
		 * @brief Computes a sample for temperature at.
		 */
		TemperatureSample sampleTemperatureAt(
			const World& world,
			const glm::dvec3& world_position,
			double world_time_seconds) const;

		/**
		 * @brief Computes a sample for radiation at.
		 */
		RadiationSample sampleRadiationAt(
			const World& world,
			const glm::dvec3& world_position,
			double world_time_seconds) const;

		/**
		 * @brief Computes a sample for gravity at.
		 */
		glm::dvec3 sampleGravityAt(const World& world, const glm::dvec3& world_position) const;
	};
}
