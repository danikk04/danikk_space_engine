#pragma once

#include <danikk_space_engine/default.h>

namespace danikk_space_engine::math
{

	/**
	 * @brief Returns the linear blend of start and end using amount without clamping the amount.
	 */
	double linearInterpolate(double start, double end, double amount);

	/**
	 * @brief Computes the quintic fade polynomial used to smooth interpolation at cell boundaries.
	 */
	double quinticFade(double value);

	/**
	 * @brief Clamps the normalized value between edges, then applies the cubic smooth-step curve.
	 */
	double smoothStep(double edge0, double edge1, double value);
}
