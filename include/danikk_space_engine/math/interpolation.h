#pragma once

#include <danikk_space_engine/default.h>

namespace danikk_space_engine::math
{

	/**
	 * @brief Performs the linear interpolate operation.
	 */
	double linearInterpolate(double start, double end, double amount);

	/**
	 * @brief Performs the quintic fade operation.
	 */
	double quinticFade(double value);

	/**
	 * @brief Performs the smooth step operation.
	 */
	double smoothStep(double edge0, double edge1, double value);
}
