#include <algorithm>

#include <danikk_space_engine/math/interpolation.h>

namespace danikk_space_engine::math
{

	/**
	 * @brief Returns the linear blend of start and end using amount without clamping the amount.
	 */
	double linearInterpolate(double start, double end, double amount)
	{
		return start + (end - start) * amount;
	}

	/**
	 * @brief Computes the quintic fade polynomial used to smooth interpolation at cell boundaries.
	 */
	double quinticFade(double value)
	{
		return value * value * value * (value * (value * 6.0 - 15.0) + 10.0);
	}

	/**
	 * @brief Clamps the normalized value between edges, then applies the cubic smooth-step curve.
	 */
	double smoothStep(double edge0, double edge1, double value)
	{
		if(edge0 == edge1) return value < edge0 ? 0.0 : 1.0;
		const double amount = std::clamp((value - edge0) / (edge1 - edge0), 0.0, 1.0);
		return amount * amount * (3.0 - 2.0 * amount);
	}
}
