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

	namespace
	{
		constexpr double base_chunk_width = 32.0;
		constexpr double chunk_cell_size = 4.0;
		constexpr double max_chunk_width = 200.0;
		constexpr double stream_recenter_distance = 16.0;

		glm::dvec3 surfacePoint(
			const glm::dvec3& up,
			const glm::dvec3& east,
			const glm::dvec3& north,
			double x,
			double y)
		{
			const glm::dvec3 direction = glm::normalize(
				up * default_planet_radius + east * x + north * y);
			const glm::dvec3 sample_position = direction * default_planet_radius;
			const double field_value = samplePlanetScalarField(sample_position);
			return direction * (default_planet_radius + field_value);
		}

		void generateSurfaceChunk(
			World& world,
			double x0,
			double y0,
			double chunk_width,
			int chunk_x,
			int chunk_y,
			WorldBodyId body_id)
		{
			// Creates a curved local grid patch from a tangent-plane rectangle. Vertices are
			// projected onto the procedural sphere, stored relative to the patch origin, and
			// shared by the two GroundTriangles emitted for each grid quad.
			Chunk& chunk = world.data.createChunk(body_id);
			const ChunkId chunk_id = chunk.chunk_id;
			chunk.grid_coordinate = glm::ivec3(chunk_x, chunk_y, 0);
			chunk.position = world.planet_patch_origin;
			const size_t cells_x = static_cast<size_t>(std::ceil(chunk_width / chunk_cell_size));
			const size_t cells_y = cells_x;
			const double step_x = chunk_width / static_cast<double>(cells_x);
			const double step_y = chunk_width / static_cast<double>(cells_y);
			DynamicArray<PointId> grid_points;
			grid_points.resize((cells_x + 1) * (cells_y + 1));

			for(size_t y_index = 0; y_index <= cells_y; y_index++)
			{
				for(size_t x_index = 0; x_index <= cells_x; x_index++)
				{
					const double local_x = x0 + static_cast<double>(x_index) * step_x;
					const double local_y = y0 + static_cast<double>(y_index) * step_y;
					const glm::dvec3 surface_position = surfacePoint(
						world.planet_patch_up,
						world.planet_patch_east,
						world.planet_patch_north,
						local_x,
						local_y);
					const PointId point_id = world.data.createPoint();
					world.data.getPoint(point_id).position = vec3(surface_position - world.planet_patch_origin);
					grid_points[y_index * (cells_x + 1) + x_index] = point_id;
				}
			}

			for(size_t y_index = 0; y_index < cells_y; y_index++)
			{
				for(size_t x_index = 0; x_index < cells_x; x_index++)
				{
					const PointId p00 = grid_points[y_index * (cells_x + 1) + x_index];
					const PointId p10 = grid_points[y_index * (cells_x + 1) + x_index + 1];
					const PointId p01 = grid_points[(y_index + 1) * (cells_x + 1) + x_index];
					const PointId p11 = grid_points[(y_index + 1) * (cells_x + 1) + x_index + 1];
					const PointId triangle_points[2][3]{{p00, p10, p11}, {p00, p11, p01}};
					for(uint8 triangle_index = 0; triangle_index < 2; triangle_index++)
					{
						const GroundTriangleId triangle_id = world.data.createGroundTriangle();
						GroundTriangle& triangle = world.data.getGroundTriangle(triangle_id);
						triangle.points[0] = triangle_points[triangle_index][0];
						triangle.points[1] = triangle_points[triangle_index][1];
						triangle.points[2] = triangle_points[triangle_index][2];
						triangle.material = default_materials::sandstone;
						world.data.addGroundTriangleToChunk(chunk_id, triangle_id);
					}
				}
			}
		}
	}

	void initializePlanetStreaming(World& world, uint32 world_body_id)
	{
		world.planet_body_id = world_body_id;
		world.planet_surface_chunk_ids.clear();
		world.planet_stream_initialized = false;
		updatePlanetStreaming(world);
	}

	void updatePlanetStreaming(World& world)
	{
		// Re-centers the local spherical chart after the camera has travelled far enough,
		// releases the previous generated patch, and builds only chunks whose tangent-plane
		// bounds intersect the 100 m surface radius. East-west chunk width grows as cos(latitude)
		// shrinks, so fewer, larger chunks cover the same nearby area close to either pole.
		if(!world.data.isWorldBodyActive(world.planet_body_id)
			|| !world.data.isPointComponentSlotActive(world.camera_component_slot_id)) return;

		const glm::dvec3 camera_position = world.getCameraWorldPosition();
		const glm::dvec3 body_position = world.data.getWorldBody(world.planet_body_id).position;
		const glm::dvec3 camera_from_center = camera_position - body_position;
		const glm::dvec3 up = glm::normalize(camera_from_center);
		const glm::dvec3 surface_origin = up * default_planet_radius;
		const double horizontal_radius = std::sqrt(up.x * up.x + up.y * up.y);
		glm::dvec3 east;
		if(horizontal_radius > 0.000001)
		{
			east = glm::dvec3(-up.y / horizontal_radius, up.x / horizontal_radius, 0.0);
		}
		else
		{
			east = glm::dvec3(1.0, 0.0, 0.0);
		}
		const glm::dvec3 north = glm::normalize(glm::cross(up, east));
		const glm::dvec3 projected_camera = up * default_planet_radius;

		bool rebuild = !world.planet_stream_initialized;
		if(world.planet_stream_initialized)
		{
			const glm::dvec3 movement = projected_camera - world.planet_stream_anchor;
			rebuild = glm::dot(movement, movement) > stream_recenter_distance * stream_recenter_distance;
		}
		if(!rebuild) return;

		for(ChunkId chunk_id : world.planet_surface_chunk_ids)
		{
			if(world.data.isChunkActive(chunk_id)) world.data.destroyChunk(chunk_id);
		}
		world.planet_surface_chunk_ids.clear();
		world.planet_patch_origin = surface_origin;
		world.planet_patch_up = up;
		world.planet_patch_east = east;
		world.planet_patch_north = north;
		world.planet_stream_anchor = projected_camera;
		world.planet_stream_initialized = true;

		const double latitude_cosine = std::max(horizontal_radius, 0.16);
		const double chunk_width = std::min(max_chunk_width, base_chunk_width / latitude_cosine);
		const double camera_x = glm::dot(camera_from_center - surface_origin, east);
		const double camera_y = glm::dot(camera_from_center - surface_origin, north);
		const int range = static_cast<int>(std::ceil(planet_chunk_stream_radius / chunk_width)) + 1;
		const double radius_squared = planet_chunk_stream_radius * planet_chunk_stream_radius;

		for(int chunk_y = -range; chunk_y <= range; chunk_y++)
		{
			for(int chunk_x = -range; chunk_x <= range; chunk_x++)
			{
				const double x0 = (static_cast<double>(chunk_x) - 0.5) * chunk_width;
				const double x1 = x0 + chunk_width;
				const double y0 = (static_cast<double>(chunk_y) - 0.5) * chunk_width;
				const double y1 = y0 + chunk_width;
				const double dx = camera_x < x0 ? x0 - camera_x : camera_x > x1 ? camera_x - x1 : 0.0;
				const double dy = camera_y < y0 ? y0 - camera_y : camera_y > y1 ? camera_y - y1 : 0.0;
				if(dx * dx + dy * dy >= radius_squared) continue;

				generateSurfaceChunk(world, x0, y0, chunk_width, chunk_x, chunk_y, world.planet_body_id);
				world.planet_surface_chunk_ids.push(world.data.getWorldBody(world.planet_body_id).chunk_ids[
					world.data.getWorldBody(world.planet_body_id).chunk_ids.size() - 1]);
			}
		}

		if(world.data.isPointComponentSlotActive(world.local_player_controller_component_slot_id))
		{
			LocalPlayerControllerComponent& controller = world.getLocalPlayerControllerComponent();
			controller.selected_faces.clear();
			controller.selected_face_count = 0;
		}
	}
}
