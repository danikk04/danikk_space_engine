#include <algorithm>

#include <danikk_space_engine/math/utility.h>

namespace danikk_space_engine::math
{

	size_t nextPowerOfTwo(size_t value)
	{
		size_t result = 1;
		while(result < value) result <<= 1;
		return result;
	}

	uint64 edgeKey(uint32 a, uint32 b)
	{
		if(a > b) std::swap(a, b);
		return (static_cast<uint64>(a) << 32) | b;
	}
}
