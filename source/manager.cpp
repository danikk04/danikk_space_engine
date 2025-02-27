#include <manager.h>
#include <component/asteroid.h>
#include <component/camera.h>
#include <component/ball_collider.h>
#include <component/voxel_map.h>
#include <component/world_object.h>

namespace sotiria_project
{
	Manager game_manager;

	void Manager::tick()
	{
		for(Object& o : map_root.childs)
		{
			o.tick(map_root);
		}
	}

	void Manager::frame()
	{
		for(Object& o : map_root.childs)
		{
			o.frame(map_root);
		}
	}

	void Manager::init()
	{
		component_class_storage.addClass<AsteroidComponent>();
		component_class_storage.addClass<CameraComponent>();
		component_class_storage.addClass<BallColliderComponent>();
		component_class_storage.addClass<VoxelMapComponent>();
		component_class_storage.addClass<WorldObjectComponent>();
	}
}
