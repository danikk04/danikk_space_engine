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
	class Controller
	{
	public:
		PointId target_point_id = 0;
		virtual void tick(WorldData& data) = 0;
		virtual void frame(World&) {}
	};

	class LocalPlayerController : public Controller
	{
	public:
		vec3 rotation = vec3(0.0f);
		DynamicArray<FacePtr> selected_polygons;

		void frame(World& world) override;
		vec3 getFront() const;
		vec3 getUp() const;
		vec3 getRight() const;
		void tick(WorldData& data) override;
	};
}
