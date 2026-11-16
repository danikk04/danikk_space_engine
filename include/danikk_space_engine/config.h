#pragma once

#include <danikk_framework/config.h>
#include <danikk_framework/path_buffer.h>
#include <danikk_framework/string.h>
#include <danikk_space_engine/default.h>

namespace danikk_space_engine
{
	enum class DefaultCameraPosition : uint8
	{
		sky,
		ground
	};

	struct Settings
	{
		bool show_chunk_bounds = true;
		bool show_face_highlight = true;
		bool show_world_body_preview = true;
		DefaultCameraPosition default_camera_position = DefaultCameraPosition::sky;
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
