#pragma once

#include <danikk_space_engine/default.h>

namespace danikk_space_engine
{
	using PointId = uint32;

	struct Point
	{
		vec3 position = vec3(0.0f);
		uint8 chunk_reference_count = 0;
	};
}
