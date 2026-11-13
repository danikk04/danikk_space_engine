#pragma once

#include <danikk_space_engine/component/point_component.h>
#include <danikk_space_engine/world/face_ptr.h>
#include <danikk_space_engine/fwd.h>

#include <danikk_framework/dynamic_array.h>

namespace danikk_space_engine
{
	class LocalPlayerController
	{
	public:
		DynamicArray<FacePtr> selected_polygons;

		void tick(World& world);
	};

	void constructLocalPlayerController(void* destination, const void* init_data);
	void moveLocalPlayerController(void* destination, void* source);
	void destroyLocalPlayerController(void* data);
	void tickLocalPlayerController(PointComponentSlot& slot, World& world);
}
