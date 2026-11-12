#pragma once

#include <danikk_space_engine/default.h>

#include <danikk_space_engine/point.h>
#include <danikk_space_engine/face_ptr.h>
#include <danikk_space_engine/chunk.h>
#include <danikk_space_engine/controller.h>
#include <danikk_space_engine/world_data.h>
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
		DynamicArray<Chunk> chunks;
		LocalPlayerController controller;

		void tick();

		void frame();

		void init();

		RaycastHit raycast(const vec3& origin, const vec3& direction, float max_distance = 10000.0f) const;
		RaycastHit raycastFromCamera(float max_distance = 10000.0f) const;
		DynamicArray<FacePtr> extrudeFaces(const DynamicArray<FacePtr>& faces, float distance);
	};

	extern World world;
}
