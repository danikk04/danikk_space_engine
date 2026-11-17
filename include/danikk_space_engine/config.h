#pragma once

#include <danikk_framework/config.h>
#include <danikk_framework/path_buffer.h>
#include <danikk_framework/string.h>
#include <danikk_space_engine/default.h>

namespace danikk_space_engine
{
	/** @brief Selects the default camera spawn location relative to the configured planet. */
	enum class DefaultCameraPosition : uint8
	{
		/** @brief Spawn the camera above the configured planet surface. */
		sky,
		/** @brief Search for a solid underground spawn point outside the protected core. */
		ground
	};

	/**
	 * @brief Stores the process-wide settings and their defaults before configuration is loaded.
	 */
	struct Settings
	{
		/** @brief Enables the chunk boundary overlay; defaults to true. */
		bool show_chunk_bounds = true;
		/** @brief Enables the translucent cube shown by Place -> Body; defaults to true. */
		bool show_world_body_preview = true;
		/** @brief Initial camera placement mode; defaults to sky for a fresh application start. */
		DefaultCameraPosition default_camera_position = DefaultCameraPosition::sky;
	};

	/**
	 * @brief Binds the singleton settings object to settings.cfg beside the executable.
	 *
	 * The file uses one `key=value` entry per line. `load()` leaves current field
	 * values unchanged for absent keys and creates a zero-byte file when none exists;
	 * on the normal first application start those values are the Settings defaults.
	 * The keys are `show_chunk_bounds`, `show_world_body_preview`,
	 * and `default_camera_position`; the latter accepts
	 * only `sky` or `ground`. `save()` rewrites the file with all supported keys
	 * and their current values, using lowercase `true`/`false` for booleans.
	 */
	class Config
	{
		danikk_framework::Config values;
		danikk_framework::PathBuffer path;

	public:
		/**
		 * @brief Resolves the executable directory and binds each supported setting key.
		 * The settings file path is `<executable directory>/settings.cfg`.
		 */
		Config();

		/**
		 * @brief Reads and validates configuration entries, retaining current values for missing keys.
		 * @return True when the file is read or successfully created empty; false on
		 *         file I/O or parse errors. Loading does not write defaults into an
		 *         existing file, reset fields after failure, or call save().
		 */
		bool load();

		/**
		 * @brief Rewrites settings.cfg with every currently supported setting.
		 * @return True when all bytes are written and the file closes successfully;
		 *         false when the file cannot be opened or the write/close fails.
		 */
		bool save() const;
	};

	/** @brief Process-wide settings fields read by the GUI, tools, and startup scenario. */
	extern Settings settings;
	/** @brief Process-wide adapter that loads and saves `settings` to settings.cfg. */
	extern Config config;
}
