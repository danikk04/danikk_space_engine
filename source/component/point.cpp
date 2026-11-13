#include <danikk_space_engine/component/point.h>
#include <danikk_space_engine/component/camera.h>
#include <danikk_space_engine/component/local_player_controller.h>

namespace danikk_space_engine
{
	namespace
	{
		const PointComponentType point_component_types[] =
		{
			{}, // none
			{
				sizeof(CameraComponent),
				alignof(CameraComponent),
				true,
				constructCameraComponent,
				moveCameraComponent,
				destroyCameraComponent,
				tickCameraComponent,
				frameCameraComponent
			},
			{
				sizeof(LocalPlayerControllerComponent),
				alignof(LocalPlayerControllerComponent),
				false,
				constructLocalPlayerControllerComponent,
				moveLocalPlayerControllerComponent,
				destroyLocalPlayerControllerComponent,
				tickLocalPlayerControllerComponent,
				nullptr
			},
			{}, // machine_gun
			{}  // engine
		};
	}

	const PointComponentType& getPointComponentType(PointComponentTypeId type_id)
	{
		const uint32 index = static_cast<uint32>(type_id);
		if(index >= static_cast<uint32>(PointComponentTypeId::count))
		{
			return point_component_types[static_cast<uint32>(PointComponentTypeId::none)];
		}
		return point_component_types[index];
	}
}
