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
		 * @brief Parses the exact `sky` or `ground` token into a camera spawn enum.
		 * @param destination Address of a DefaultCameraPosition field.
		 * @param value Non-null configuration value bytes.
		 * @param value_size Number of bytes in value; no terminator is required.
		 * @return True for one of the two recognized tokens; false leaves the field unchanged.
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

		/**
		 * @brief Parses a mesh generator token into the global setting.
		 * @param destination Address of a PlanetMeshGenerator setting.
		 * @param value Configuration value bytes, without a required null terminator.
		 * @param value_size Number of bytes in value.
		 * @return True for a supported generator token; false otherwise.
		 */
		bool parsePlanetMeshGenerator(void* destination, const char* value, size_t value_size)
		{
			constexpr char marching_tetrahedra[] = "marching_tetrahedra";
			constexpr char surface_nets[] = "surface_nets";
			if(value_size == sizeof(marching_tetrahedra) - 1
				&& std::memcmp(value, marching_tetrahedra, sizeof(marching_tetrahedra) - 1) == 0)
			{
				*static_cast<PlanetMeshGenerator*>(destination) = PlanetMeshGenerator::marching_tetrahedra;
				return true;
			}
			if(value_size == sizeof(surface_nets) - 1
				&& std::memcmp(value, surface_nets, sizeof(surface_nets) - 1) == 0)
			{
				*static_cast<PlanetMeshGenerator*>(destination) = PlanetMeshGenerator::surface_nets;
				return true;
			}
			return false;
		}
	}

	/**
	 * @brief Resolves settings.cfg beside the executable and binds all settings keys.
	 * Boolean parsing is delegated to the framework Config; camera placement uses
	 * the local exact-token parser for `sky` and `ground`.
	 */
	Config::Config()
		: path(danikk_framework::getExecutableDirectory())
	{
		path.setFileName("settings.cfg");
		values.bind("show_chunk_bounds", settings.show_chunk_bounds);
		values.bind("show_world_body_preview", settings.show_world_body_preview);
		values.bind("default_camera_position", &settings.default_camera_position, &parseDefaultCameraPosition);
		values.bind("planet_mesh_generator", &settings.planet_mesh_generator, &parsePlanetMeshGenerator);
	}

	/**
	 * @brief Reads settings.cfg into the bound fields, preserving defaults for omitted keys.
	 * @return True on successful read or empty-file creation; false on open,
	 *         seek, read, or framework parsing failure. Parse failures report the
	 *         line number supplied by the framework Config object.
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
	 * @brief Serializes every setting as a `key=value` line and replaces settings.cfg.
	 * @return True only when the full buffer is written and fclose succeeds.
	 */
	bool Config::save() const
	{
		danikk_framework::String contents;
		contents << "show_chunk_bounds=" << (settings.show_chunk_bounds ? "true" : "false") << '\n';
		contents << "show_world_body_preview=" << (settings.show_world_body_preview ? "true" : "false") << '\n';
		contents << "default_camera_position="
			<< (settings.default_camera_position == DefaultCameraPosition::ground ? "ground" : "sky") << '\n';
		contents << "planet_mesh_generator="
			<< (settings.planet_mesh_generator == PlanetMeshGenerator::surface_nets
				? "surface_nets" : "marching_tetrahedra") << '\n';

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
