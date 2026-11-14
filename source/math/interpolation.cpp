#include <algorithm>

#include <danikk_space_engine/math/interpolation.h>

namespace danikk_space_engine::math
{
	double linearInterpolate(double start, double end, double amount)
	{
		return start + (end - start) * amount;
	}

	double quinticFade(double value)
	{
		return value * value * value * (value * (value * 6.0 - 15.0) + 10.0);
	}

	double smoothStep(double edge0, double edge1, double value)
	{
		if(edge0 == edge1) return value < edge0 ? 0.0 : 1.0;
		const double amount = std::clamp((value - edge0) / (edge1 - edge0), 0.0, 1.0);
		return amount * amount * (3.0 - 2.0 * amount);
	}
}
