#pragma once

#include <danikk_space_engine/id.h>
#include <danikk_space_engine/component/point/point.h>

namespace danikk_space_engine
{
	struct NpcControllerComponent
	{
		PointComponentSlotId character_slot_id = static_cast<PointComponentSlotId>(-1);
		vec3 target_position = vec3(0.0f); // Same chunk-local coordinates as the character point.
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

		void moveTo(const vec3& position)
		{
			wander_enabled = false;
			target_position = position;
			has_movement_target = true;
		}
		void stop()
		{
			wander_enabled = false;
			has_movement_target = false;
		}
	};

	PointComponentSlotId createNpc(World& world, WorldBodyId world_body_id,
		const glm::dvec3& position, const vec3& up, const vec3& forward);

	void constructNpcControllerComponent(void* destination, const void* init_data);
	void moveNpcControllerComponent(void* destination, void* source);
	void destroyNpcControllerComponent(void* data);
	void tickNpcControllerComponent(PointComponentSlot& slot, World& world);
}
