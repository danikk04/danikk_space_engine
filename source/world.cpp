#include <danikk_framework/glm.h>

#include <danikk_space_engine/world.h>
#include <danikk_space_engine/asset.h>

namespace danikk_space_engine
{
	World world;

	void World::tick()
	{
		controller.tick(points);
	}

	void World::frame()
	{
		static_asset_collection.base3d_shader.use();
		const vec3 camera_position = points[controller.target_point_id].position;
		const vec3 camera_front = controller.getFront();
		const mat4 view = glm::lookAt(camera_position, camera_position + camera_front, controller.getUp());
		const mat4 projection = glm::perspective(90.0f, screen_ratio_gz, 0.0001f, 10000.0f);
		setViewMatrix(view);
		setProjectionMatrix(projection);
	}

	void World::init()
	{
		points.clear();
		Point& camera_point = points.pushCtor();
		controller.target_point_id = static_cast<PointId>(points.size() - 1);
		camera_point.position = vec3(0.0f);
		controller.rotation = vec3(0.0f);
	}
}
