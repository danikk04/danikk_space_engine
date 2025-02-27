#include <default.h>
#include <component/component.h>
#include <component/voxel_map.h>
#include <component/asteroid.h>
#include <component/camera.h>
#include <component/world_object.h>
#include <component/ball_collider.h>

namespace sotiria_project
{
	uint32 last_component_id = 1;
	IMPLEMENT_COMPONENT_ID(VoxelMapComponent);
	IMPLEMENT_COMPONENT_ID(AsteroidComponent);
	IMPLEMENT_COMPONENT_ID(CameraComponent);
	IMPLEMENT_COMPONENT_ID(WorldObjectComponent);
	IMPLEMENT_COMPONENT_ID(BallColliderComponent);
}
