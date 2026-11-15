#pragma once

#include <danikk_framework/config.h>
#include <danikk_framework/path_buffer.h>
#include <danikk_framework/string.h>

namespace danikk_space_engine
{
	struct Settings
	{
		bool show_chunk_bounds = true;
		bool show_face_highlight = true;
		bool show_volume_cell_preview = true;
	};

	class Config
	{
		danikk_framework::Config values;
		danikk_framework::PathBuffer path;

	public:
		Config();

		bool load();
		bool save() const;
	};

	extern Settings settings;
	extern Config config;
}
