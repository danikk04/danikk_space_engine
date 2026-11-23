#pragma once

#include <danikk_space_engine/default.h>

namespace danikk_space_engine::math
{

	/**
	 * @brief Returns the smallest power of two not less than the requested value.
	 * @param value Minimum requested capacity; zero returns one.
	 * @return Power-of-two capacity.
	 */
	size_t nextPowerOfTwo(size_t value);

	/**
	 * @brief Packs an unordered pair of 32-bit IDs into a stable 64-bit key.
	 * @param a First endpoint ID.
	 * @param b Second endpoint ID.
	 * @return The same key regardless of endpoint order.
	 */
	uint64 edgeKey(uint32 a, uint32 b);
}
