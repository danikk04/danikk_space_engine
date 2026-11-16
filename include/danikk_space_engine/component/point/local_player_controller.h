#pragma once

#include <danikk_space_engine/component/point/point.h>
#include <danikk_space_engine/world/face_ptr.h>
#include <danikk_space_engine/fwd.h>
#include <danikk_space_engine/tool.h>

#include <danikk_framework/dynamic_array.h>

namespace danikk_space_engine
{

	/**
	 * @brief Processes player input, face selection, and active tool actions.
	 */
	class LocalPlayerControllerComponent
	{
	public:
		DynamicArray<FacePtr> selected_faces;
		size_t selected_face_count = 0;
		ToolId active_tool = ToolId::none;
		float tool_scroll_accumulator = 0.0f;
		ToolActionId selected_tool_action = ToolActionId::none;
		float tool_action_scroll_accumulator = 0.0f;
		float camera_movement_speed = 1.0f;
		WorldBodyId dragged_world_body_id = static_cast<WorldBodyId>(-1);
		double dragged_body_distance = 0.0;
		glm::dvec3 dragged_hit_offset = glm::dvec3(0.0);
		PointId dragged_human_point_id = static_cast<PointId>(-1);
		ChunkId dragged_human_chunk_id = static_cast<ChunkId>(-1);
		PointComponentSlotId dragged_human_slot_id = static_cast<PointComponentSlotId>(-1);
		double dragged_human_distance = 0.0;
		bool dragged_human_followed_ground = true;

		/**
		 * @brief Advances the world simulation by one fixed update step.
		 */
		void tick(World& world);
	};

	/**
	 * @brief Constructs local player controller component.
	 */
	void constructLocalPlayerControllerComponent(void* destination, const void* init_data);

	/**
	 * @brief Moves local player controller component.
	 */
	void moveLocalPlayerControllerComponent(void* destination, void* source);

	/**
	 * @brief Removes and releases local player controller component.
	 */
	void destroyLocalPlayerControllerComponent(void* data);

	/**
	 * @brief Runs one simulation tick for local player controller component.
	 */
	void tickLocalPlayerControllerComponent(PointComponentSlot& slot, World& world);
}
