#include <danikk_space_engine/config.h>

#include <cstring>

#include <danikk_framework/log.h>

namespace danikk_space_engine
{
	Settings settings;
	Config config;

	namespace
	{

		/**
		 * @brief Parses default camera position.
		 */
		bool parseDefaultCameraPosition(void* destination, const char* value, size_t value_size)
		{
			if(value_size == 3 && std::memcmp(value, "sky", 3) == 0)
			{
				*static_cast<DefaultCameraPosition*>(destination) = DefaultCameraPosition::sky;
				return true;
			}
			if(value_size == 6 && std::memcmp(value, "ground", 6) == 0)
			{
				*static_cast<DefaultCameraPosition*>(destination) = DefaultCameraPosition::ground;
				return true;
			}
			return false;
		}
	}

	/**
	 * @brief Initializes the configuration path and binds settings fields for loading and saving.
	 */
	Config::Config()
		: path(danikk_framework::getExecutableDirectory())
	{
		path.setFileName("settings.cfg");
		values.bind("show_chunk_bounds", settings.show_chunk_bounds);
		values.bind("show_face_highlight", settings.show_face_highlight);
		values.bind("show_world_body_preview", settings.show_world_body_preview);
		values.bind("default_camera_position", &settings.default_camera_position, &parseDefaultCameraPosition);
	}

	/**
	 * @brief Performs the load operation.
	 */
	bool Config::load()
	{
		std::FILE* file = std::fopen(path.c_string(), "rb");
		if(file == nullptr)
		{
			file = std::fopen(path.c_string(), "wb");
			if(file == nullptr)
			{
				danikk_framework::formatLogError("Unable to create config file: %", path.c_string());
				return false;
			}
			std::fclose(file);
			return true;
		}

		if(std::fseek(file, 0, SEEK_END) != 0)
		{
			std::fclose(file);
			danikk_framework::formatLogError("Unable to seek config file: %", path.c_string());
			return false;
		}
		const long file_size = std::ftell(file);

		if(file_size < 0 || std::fseek(file, 0, SEEK_SET) != 0)
		{
			std::fclose(file);
			danikk_framework::formatLogError("Unable to read config file: %", path.c_string());
			return false;
		}

		danikk_framework::String contents(static_cast<size_t>(file_size));
		const size_t bytes_read = std::fread(contents.data(), 1, contents.size(), file);
		const bool read_succeeded = bytes_read == contents.size() && std::ferror(file) == 0;
		std::fclose(file);
		if(!read_succeeded)
		{
			danikk_framework::formatLogError("Unable to read config file: %", path.c_string());
			return false;
		}

		if(!values.load(contents))
		{
			danikk_framework::formatLogError(
				"Invalid config file at line %",
				values.last_error_line);
			return false;
		}
		return true;
	}

	/**
	 * @brief Performs the save operation.
	 */
	bool Config::save() const
	{
		danikk_framework::String contents;
		contents << "show_chunk_bounds=" << (settings.show_chunk_bounds ? "true" : "false") << '\n';
		contents << "show_face_highlight=" << (settings.show_face_highlight ? "true" : "false") << '\n';
		contents << "show_world_body_preview=" << (settings.show_world_body_preview ? "true" : "false") << '\n';
		contents << "default_camera_position="
			<< (settings.default_camera_position == DefaultCameraPosition::ground ? "ground" : "sky") << '\n';

		std::FILE* file = std::fopen(path.c_string(), "wb");
		if(file == nullptr)
		{
			danikk_framework::formatLogError("Unable to save config file: %", path.c_string());
			return false;
		}
		const size_t bytes_written = std::fwrite(contents.data(), 1, contents.size(), file);
		const int close_result = std::fclose(file);
		const bool write_succeeded = bytes_written == contents.size() && close_result == 0;
		if(!write_succeeded)
		{
			danikk_framework::formatLogError("Unable to save config file: %", path.c_string());
			return false;
		}
		return true;
	}
}
