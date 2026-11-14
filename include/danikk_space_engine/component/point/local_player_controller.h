#pragma once

#include <danikk_space_engine/component/point/point.h>
#include <danikk_space_engine/world/face_ptr.h>
#include <danikk_space_engine/fwd.h>
#include <danikk_space_engine/tool.h>

#include <danikk_framework/dynamic_array.h>

namespace danikk_space_engine
{
	class LocalPlayerControllerComponent
	{
	public:
		DynamicArray<FacePtr> selected_faces;
		size_t selected_face_count = 0;
		ToolId active_tool = ToolId::none;
		float tool_scroll_accumulator = 0.0f;
		BuildActionId selected_build_action = BuildActionId::none;
		float build_action_scroll_accumulator = 0.0f;
		WorldBodyId dragged_world_body_id = static_cast<WorldBodyId>(-1);
		double dragged_body_distance = 0.0;
		glm::dvec3 dragged_hit_offset = glm::dvec3(0.0);

		void tick(World& world);
	};

	void constructLocalPlayerControllerComponent(void* destination, const void* init_data);
	void moveLocalPlayerControllerComponent(void* destination, void* source);
	void destroyLocalPlayerControllerComponent(void* data);
	void tickLocalPlayerControllerComponent(PointComponentSlot& slot, World& world);
}
