#pragma once

#include <danikk_space_engine/point.h>
#include <danikk_space_engine/world/face_ptr.h>
#include <danikk_space_engine/world/volume_cell.h>
#include <danikk_space_engine/world/world_data.h>
#include <danikk_space_engine/fwd.h>

#include <danikk_framework/glm.h>
#include <danikk_framework/dynamic_array.h>

#include <danikk_space_engine/default.h>

namespace danikk_space_engine
{
	class LocalPlayerController
	{
	public:
		DynamicArray<FacePtr> selected_polygons;

		void tick(World& world);
	};
}
