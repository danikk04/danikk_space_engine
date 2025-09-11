#pragma once

#include <default.h>
#include <mode_container.h>
#include <danikk_engine/texture.h>

namespace danikk_space_engine
{
	enum border_display_style
	{
		none,
		only_chunk_frame,
		only_region_frame,
		all_frame,

		BASE = none,
		MAX = 4
	};

	extern const char* border_display_style_strings[];

	declare_mode(border_display_style);
}
