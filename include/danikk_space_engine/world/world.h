#pragma once

#include <danikk_space_engine/default.h>

#include <danikk_space_engine/point.h>
#include <danikk_space_engine/component/camera.h>
#include <danikk_space_engine/world/face_ptr.h>
#include <danikk_space_engine/world/chunk.h>
#include <danikk_space_engine/controller.h>
#include <danikk_space_engine/world/world_data.h>
#include <danikk_framework/dynamic_array.h>

namespace danikk_space_engine
{
	struct RaycastHit
	{
		bool hit = false;
		float distance = 0.0f;
		vec3 position = vec3(0.0f);
		vec3 normal = vec3(0.0f);
		FacePtr face;
		uint8 triangle_index = 0;
		PointId points[3]{};
	};

	class World
	{
	public:
		WorldData data;
		PointComponentSlotId camera_component_slot_id = 0;
		PointComponentSlotId local_player_controller_slot_id = 0;

		void tick();

		void frame();

		void init();

		RaycastHit raycast(const vec3& origin, const vec3& direction, float max_distance = 10000.0f) const;
		RaycastHit raycastFromCamera(float max_distance = 10000.0f) const;
		PointComponentSlot& getCameraSlot();
		const PointComponentSlot& getCameraSlot() const;
		CameraComponent& getCameraComponent();
		const CameraComponent& getCameraComponent() const;
		Point& getCameraPoint();
		const Point& getCameraPoint() const;
		LocalPlayerController& getLocalPlayerController();
		const LocalPlayerController& getLocalPlayerController() const;
		DynamicArray<FacePtr> extrudeFaces(const DynamicArray<FacePtr>& faces, float distance);
		void deleteVolumeCells(const DynamicArray<FacePtr>& faces);
	};

	extern World world;
}
