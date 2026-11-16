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

	/**
	 * @brief Stores the current application settings.
	 */
	struct Settings
	{
		bool show_chunk_bounds = true;
		bool show_face_highlight = true;
		bool show_world_body_preview = true;
		DefaultCameraPosition default_camera_position = DefaultCameraPosition::sky;
	};

	/**
	 * @brief Binds settings fields to configuration file loading and saving.
	 */
	class Config
	{
		danikk_framework::Config values;
		danikk_framework::PathBuffer path;

	public:
		Config();

		/**
		 * @brief Reads and validates configuration entries, retaining the in-memory defaults for missing keys.
		 */
		bool load();

		/**
		 * @brief Writes the current bound settings values to the configuration file.
		 */
		bool save() const;
	};

	extern Settings settings;
	extern Config config;
}
