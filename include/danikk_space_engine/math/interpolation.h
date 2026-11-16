#pragma once

#include <danikk_space_engine/default.h>

namespace danikk_space_engine::math
{

	/**
	 * @brief Returns the linear blend of two values without clamping the interpolation amount.
	 * @param start Value at amount 0.
	 * @param end Value at amount 1.
	 * @param amount Blend factor; values outside [0, 1] extrapolate.
	 * @return start + (end - start) * amount.
	 */
	double linearInterpolate(double start, double end, double amount);

	/**
	 * @brief Evaluates the quintic fade polynomial 6t^5 - 15t^4 + 10t^3.
	 * @param value Input parameter, conventionally in [0, 1]; it is not clamped.
	 * @return The polynomial result. Its value is 0 at t=0 and 1 at t=1;
	 *         its first and second derivatives vanish at both endpoints.
	 */
	double quinticFade(double value);

	/**
	 * @brief Maps a value between two edges to [0, 1], then applies cubic smooth-step.
	 * @param edge0 Lower edge of the interval.
	 * @param edge1 Upper edge of the interval.
	 * @param value Value to normalize and clamp.
	 * @return Smoothed normalized value. If the edges are equal, returns 0 below
	 *         the edge and 1 at or above it.
	 */
	double smoothStep(double edge0, double edge1, double value);
}
