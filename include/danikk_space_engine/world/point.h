#pragma once

#include <danikk_space_engine/default.h>
#include <danikk_space_engine/id.h>

namespace danikk_space_engine
{
	struct Point
	{
		vec3 position = vec3(0.0f);
		// Number of distinct chunks whose point_ids arrays contain this point.
		uint8 referencing_chunk_count = 0;
	};
}
