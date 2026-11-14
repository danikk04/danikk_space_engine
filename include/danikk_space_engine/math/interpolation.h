#pragma once

#include <danikk_space_engine/default.h>

namespace danikk_space_engine::math
{
	double linearInterpolate(double start, double end, double amount);
	double quinticFade(double value);
	double smoothStep(double edge0, double edge1, double value);
}
