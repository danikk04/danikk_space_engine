#pragma once

#include <danikk_space_engine/point.h>

#include <danikk_framework/glm.h>
#include <danikk_framework/dynamic_array.h>

#include <danikk_space_engine/default.h>

namespace danikk_space_engine
{
	class World;

	class Controller
	{
	public:
		PointId target_point_id = 0;
		virtual void tick(DynamicArray<Point>& points) = 0;
		virtual void frame(const World&) {}
	};

	class LocalPlayerController : public Controller
	{
	public:
		struct SelectedPolygon
		{
			uint32 region_index = 0;
			uint32 cell_index = 0;
			uint8 face_index = 0;

			SelectedPolygon() = default;

			SelectedPolygon(uint32 region_index, uint32 cell_index, uint8 face_index)
				: region_index(region_index), cell_index(cell_index), face_index(face_index) {}
		};

		vec3 rotation = vec3(0.0f);
		DynamicArray<SelectedPolygon> selected_polygons;

		void frame(const World& world) override;
		vec3 getFront() const;
		vec3 getUp() const;
		vec3 getRight() const;
		void tick(DynamicArray<Point>& points) override;
	};
}
