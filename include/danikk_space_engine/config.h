#pragma once

#include <danikk_framework/config.h>
#include <danikk_framework/path_buffer.h>
#include <danikk_framework/string.h>
#include <danikk_space_engine/component/world_body/planet/planet.h>

namespace danikk_space_engine
{
	struct Settings
	{
		bool show_chunk_bounds = true;
		bool show_face_highlight = true;
		bool show_world_body_preview = true;
		double planet_radius = default_planet_radius;
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
