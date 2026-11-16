#pragma once

#include <danikk_space_engine/id.h>
#include <danikk_space_engine/component/point/point.h>

namespace danikk_space_engine
{

	/**
	 * @brief Stores the NPC movement, look direction, and autonomous behavior state.
	 */
	struct NpcControllerComponent
	{
		PointComponentSlotId human_slot_id = static_cast<PointComponentSlotId>(-1);
		vec3 target_position = vec3(0.0f); // Same chunk-local coordinates as the human point.
		bool has_movement_target = false;
		bool look_at_camera = false;
		bool enabled = true;
		bool wander_enabled = true;
		bool wander_initialized = false;
		vec3 wander_center = vec3(0.0f);
		float wander_radius = 4.0f;
		float wander_timer = 0.0f;
		float look_timer = 0.0f;
		vec2 look_target = vec2(0.0f);
		uint32 random_state = 0;

		/**
		 * @brief Moves to.
		 */
		void moveTo(const vec3& position)
		{
			wander_enabled = false;
			target_position = position;
			has_movement_target = true;
		}

		/**
		 * @brief Signals the chunk worker to stop and waits for its thread to finish.
		 */
		void stop()
		{
			wander_enabled = false;
			has_movement_target = false;
		}
	};

	/**
	 * @brief Creates a human character, then attaches the autonomous NPC controller to its point.
	 */
	PointComponentSlotId createNpc(World& world, WorldBodyId world_body_id,
		const glm::dvec3& position, const vec3& up, const vec3& forward);

	/**
	 * @brief Constructs npc controller component.
	 */
	void constructNpcControllerComponent(void* destination, const void* init_data);

	/**
	 * @brief Moves npc controller component.
	 */
	void moveNpcControllerComponent(void* destination, void* source);

	/**
	 * @brief Removes and releases npc controller component.
	 */
	void destroyNpcControllerComponent(void* data);

	/**
	 * @brief Runs one simulation tick for npc controller component.
	 */
	void tickNpcControllerComponent(PointComponentSlot& slot, World& world);
}
