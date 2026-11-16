#include <algorithm>

#include <danikk_space_engine/math/interpolation.h>

namespace danikk_space_engine::math
{

	/**
	 * @brief Linearly blends two endpoints and permits extrapolation.
	 * @param start Value at interpolation amount 0.
	 * @param end Value at interpolation amount 1.
	 * @param amount Unclamped blend factor.
	 * @return start + (end - start) * amount.
	 */
	double linearInterpolate(double start, double end, double amount)
	{
		return start + (end - start) * amount;
	}

	/**
	 * @brief Evaluates the quintic interpolation fade curve without clamping its input.
	 * @param value Curve parameter, normally between 0 and 1.
	 * @return 6t^5 - 15t^4 + 10t^3.
	 */
	double quinticFade(double value)
	{
		return value * value * value * (value * (value * 6.0 - 15.0) + 10.0);
	}

	/**
	 * @brief Clamps a normalized interval position and applies the cubic smooth-step polynomial.
	 * @param edge0 Lower interval edge.
	 * @param edge1 Upper interval edge.
	 * @param value Input to normalize.
	 * @return Cubic-smoothed value in [0, 1], with a threshold result when the edges match.
	 */
	double smoothStep(double edge0, double edge1, double value)
	{
		if(edge0 == edge1) return value < edge0 ? 0.0 : 1.0;
		const double amount = std::clamp((value - edge0) / (edge1 - edge0), 0.0, 1.0);
		return amount * amount * (3.0 - 2.0 * amount);
	}
}
