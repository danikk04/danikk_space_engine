#include <algorithm>
#include <cmath>

#include <danikk_space_engine/world/planet.h>
#include <danikk_space_engine/world/world.h>
#include <danikk_space_engine/component/point/local_player_controller.h>

namespace danikk_space_engine
{
	double samplePlanetScalarField(const glm::dvec3& position_relative_to_planet_center, double radius)
	{
		const double x = position_relative_to_planet_center.x;
		const double y = position_relative_to_planet_center.y;
		const double z = position_relative_to_planet_center.z;
		const double terrain_height =
			7.0 * std::sin(x * 0.006) * std::sin(y * 0.005)
			+ 4.0 * std::sin(z * 0.009 + x * 0.004)
			+ 2.0 * std::sin(y * 0.018 + z * 0.011);
		return radius + terrain_height - glm::length(position_relative_to_planet_center);
	}

	MaterialId samplePlanetMaterial(const glm::dvec3& position_relative_to_planet_center)
	{
		const double x = position_relative_to_planet_center.x;
		const double y = position_relative_to_planet_center.y;
		const double z = position_relative_to_planet_center.z;
		const double geological_field =
			std::sin(x * 0.013 + z * 0.006)
			+ std::cos(y * 0.011 - x * 0.004)
			+ 0.5 * std::sin(z * 0.017 + y * 0.008);
		return geological_field > 0.65
			? default_materials::granite
			: default_materials::sandstone;
	}

	namespace
	{
		constexpr int planet_chunks_per_face = 32;
		constexpr double stream_update_distance = 8.0;

		struct PlanetCubeFace
		{
			glm::dvec3 normal;
			glm::dvec3 right;
			glm::dvec3 up;
		};

		PlanetCubeFace getPlanetCubeFace(int face_index)
		{
			switch(face_index)
			{
				case 0: return {glm::dvec3( 1, 0, 0), glm::dvec3( 0, 1, 0), glm::dvec3(0, 0, 1)};
				case 1: return {glm::dvec3(-1, 0, 0), glm::dvec3( 0,-1, 0), glm::dvec3(0, 0, 1)};
				case 2: return {glm::dvec3( 0, 1, 0), glm::dvec3(-1, 0, 0), glm::dvec3(0, 0, 1)};
				case 3: return {glm::dvec3( 0,-1, 0), glm::dvec3( 1, 0, 0), glm::dvec3(0, 0, 1)};
				case 4: return {glm::dvec3( 0, 0, 1), glm::dvec3( 1, 0, 0), glm::dvec3(0, 1, 0)};
				default:return {glm::dvec3( 0, 0,-1), glm::dvec3(-1, 0, 0), glm::dvec3(0, 1, 0)};
			}
		}

		glm::dvec3 planetSurfacePosition(const PlanetCubeFace& face, double x, double y)
		{
			const glm::dvec3 direction = glm::normalize(
				face.normal * default_planet_radius
					+ face.right * x
					+ face.up * y);
			const double height = samplePlanetScalarField(direction * default_planet_radius);
			return direction * (default_planet_radius + height);
		}

	}

	void initializePlanetStreaming(World& world, uint32 world_body_id)
	{
		world.planet_body_id = world_body_id;
		world.planet_generation_id++;
		world.planet_chunks.clear();
		world.desired_planet_chunk_keys.clear();
		world.planet_stream_initialized = false;
		world.chunk_loader.updatePendingRequests(world.planet_generation_id, {});
		updatePlanetStreaming(world);
	}

	void updatePlanetStreaming(World& world)
	{
		// Uses six fixed cube-sphere faces to give each surface chunk a stable key. It computes the
		// nearby key set, keeps chunks that remain in range, unloads only chunks outside the set, and
		// requests only missing chunks. The loader caches generated point and triangle data by key.
		if(!world.data.isWorldBodyActive(world.planet_body_id)
			|| !world.data.isPointComponentSlotActive(world.camera_component_slot_id)) return;

		const glm::dvec3 camera_position = world.getCameraWorldPosition();
		const glm::dvec3 body_position = world.data.getWorldBody(world.planet_body_id).position;
		const glm::dvec3 camera_direction = glm::normalize(camera_position - body_position);

		bool rebuild = !world.planet_stream_initialized;
		if(world.planet_stream_initialized)
		{
			const double direction_dot = glm::clamp(glm::dot(camera_direction, world.planet_stream_anchor), -1.0, 1.0);
			const double surface_distance = std::acos(direction_dot) * default_planet_radius;
			rebuild = surface_distance > stream_update_distance;
		}
		if(!rebuild) return;
		world.planet_stream_anchor = camera_direction;
		world.planet_stream_initialized = true;

		const double chunk_width = 2.0 * default_planet_radius / planet_chunks_per_face;
		const double tile_radius = chunk_width * 0.72;
		const double max_center_distance = planet_chunk_stream_radius + tile_radius;
		const double min_center_dot = std::cos(max_center_distance / default_planet_radius);
		std::vector<glm::ivec3> desired_keys;
		std::vector<ChunkGenerationRequest> requests;
		for(int face_index = 0; face_index < 6; face_index++)
		{
			const PlanetCubeFace face = getPlanetCubeFace(face_index);
			for(int chunk_y = 0; chunk_y < planet_chunks_per_face; chunk_y++)
			{
				for(int chunk_x = 0; chunk_x < planet_chunks_per_face; chunk_x++)
				{
					const double x0 = -default_planet_radius + chunk_x * chunk_width;
					const double y0 = -default_planet_radius + chunk_y * chunk_width;
					const double center_x = x0 + chunk_width * 0.5;
					const double center_y = y0 + chunk_width * 0.5;
					const glm::dvec3 center_direction = glm::normalize(
						face.normal * default_planet_radius
							+ face.right * center_x
							+ face.up * center_y);
					if(glm::dot(center_direction, camera_direction) < min_center_dot) continue;

					const glm::ivec3 key(chunk_x, chunk_y, face_index);
					desired_keys.push_back(key);
					bool already_loaded = false;
					for(const PlanetChunkRecord& record : world.planet_chunks)
					{
						if(record.grid_coordinate.x == key.x
							&& record.grid_coordinate.y == key.y
						&& record.grid_coordinate.z == key.z)
					{
						already_loaded = true;
						break;
						}
					}
					if(already_loaded) continue;

					ChunkGenerationRequest request;
					request.generation_id = world.planet_generation_id;
					request.world_body_id = world.planet_body_id;
					request.grid_coordinate = key;
					request.chunk_position = planetSurfacePosition(face, center_x, center_y);
					request.up = face.normal;
					request.east = face.right;
					request.north = face.up;
					request.x0 = x0;
					request.y0 = y0;
					request.width = chunk_width;
					requests.push_back(request);
				}
			}
		}

		for(size_t chunk_index = 0; chunk_index < world.planet_chunks.size();)
		{
			PlanetChunkRecord& record = world.planet_chunks[chunk_index];
			bool keep_chunk = false;
			for(const glm::ivec3& key : desired_keys)
			{
				if(record.grid_coordinate.x == key.x
					&& record.grid_coordinate.y == key.y
					&& record.grid_coordinate.z == key.z)
				{
					keep_chunk = true;
					break;
				}
			}
			if(keep_chunk)
			{
				chunk_index++;
				continue;
			}
			if(world.data.isChunkActive(record.chunk_id)) world.data.destroyChunk(record.chunk_id);
			for(size_t shift_index = chunk_index; shift_index + 1 < world.planet_chunks.size(); shift_index++)
			{
				world.planet_chunks[shift_index] = world.planet_chunks[shift_index + 1];
			}
			world.planet_chunks.resize(world.planet_chunks.size() - 1);
		}

		world.desired_planet_chunk_keys.clear();
		for(const glm::ivec3& key : desired_keys) world.desired_planet_chunk_keys.push(key);
		world.chunk_loader.updatePendingRequests(world.planet_generation_id, requests);

		if(world.data.isPointComponentSlotActive(world.local_player_controller_component_slot_id))
		{
			LocalPlayerControllerComponent& controller = world.getLocalPlayerControllerComponent();
			controller.selected_faces.clear();
			controller.selected_face_count = 0;
		}
	}
}
