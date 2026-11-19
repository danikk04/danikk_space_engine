#pragma once

#include <glm/glm.hpp>

namespace danikk_space_engine
{

	/**
	 * @brief Forward declaration used by environmental sampling interfaces.
	 */
	class World;

	/**
	 * @brief Holds a temperature result and distinguishes an unavailable field from a valid zero.
	 */
	struct TemperatureSample
	{
		/** @brief Sampled absolute temperature in kelvin when available. */
		double temperature_kelvin = 0.0;
		/** @brief False until a temperature provider is implemented for the sampled position. */
		bool available = false;
	};

	/**
	 * @brief Holds a radiation dose-rate result and its availability state.
	 */
	struct RadiationSample
	{
		/** @brief Sampled dose rate in sieverts per second when available. */
		double radiation_dose_rate_sv_per_second = 0.0;
		/** @brief False when no radiation provider can supply a value at the position. */
		bool available = false;
	};

	/**
	 * @brief Samples temperature, radiation, and gravitational acceleration at world positions.
	 *
	 * The class keeps a reference to its owning World and evaluates each requested position on demand. Temperature and
	 * radiation currently have no providers and return unavailable samples. Gravity sums every
	 * active planet component: magnitude follows inverse-square falloff outside the nominal radius
	 * and `surface_gravity * radius / distance` inside it. The interior formula therefore grows
	 * toward the center; samples within 1e-6 world units of a center are skipped to avoid
	 * division by zero and unstable directions.
	 */
	class WorldEnvironment
	{
		/** @brief Owning World whose environmental sources are sampled. */
		World& world;

	public:
		/** @brief Binds this environment sampler to its owning World. */
		explicit WorldEnvironment(World& world) : world(world) {}

		/**
		 * @brief Samples the temperature field at a world-space position and time.
	 *
	 * No temperature source is implemented yet, so the input position and time are currently
	 * ignored and the returned sample remains unavailable.
		 * @param world_position Position to sample in global double-precision coordinates.
		 * @param world_time_seconds Simulation time available to time-dependent providers.
		 * @return Currently unavailable with a zero-valued temperature; availability
		 *         distinguishes this from a physical reading of 0 K.
		 */
		TemperatureSample sampleTemperatureAt(
			const glm::dvec3& world_position,
			double world_time_seconds) const;

		/**
		 * @brief Samples the radiation dose-rate field at a world-space position and time.
	 *
	 * No radiation source is implemented yet, so the input position and time are currently
	 * ignored and the returned sample remains unavailable.
		 * @param world_position Position to sample in global double-precision coordinates.
		 * @param world_time_seconds Simulation time available to time-dependent providers.
		 * @return Currently unavailable with a zero-valued dose rate.
		 */
		RadiationSample sampleRadiationAt(
			const glm::dvec3& world_position,
			double world_time_seconds) const;

		/**
		 * @brief Sums gravity vectors from every active planet component at a world position.
	 *
	 * Each contribution points from the sample toward the planet center. Outside radius `R`,
	 * its magnitude is `surface_gravity * (R / d)^2`; inside `R`, it is
	 * `surface_gravity * (R / d)`. This interior rule is an implementation choice rather than
		 * the field of a uniform-density sphere, and points within 1e-6 units of a center contribute
		 * nothing.
		 * @param world_position Global position where acceleration is sampled.
		 * @return Acceleration vector in m/s² directed toward each contributing planet.
		 * Planets with non-positive radius or surface gravity and samples within 1e-6 units of
		 * their center are skipped. This method evaluates position only; it does not use simulation time.
		 */
		glm::dvec3 sampleGravityAt(const glm::dvec3& world_position) const;
	};
}
