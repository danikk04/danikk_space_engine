#pragma once

#include <danikk_space_engine/component/point/point.h>
#include <danikk_space_engine/id.h>
#include <danikk_engine/mesh.h>

namespace danikk_space_engine
{
	enum class HumanRenderType : uint8
	{
		minecraft_style,
		model_3d
	};

	struct HumanAppearance
	{
		HumanRenderType render_type = HumanRenderType::minecraft_style;
		HumanSkinId skin_id = 0;
	};

	enum class HumanPart : uint8
	{
		head, torso, right_arm, left_arm, right_leg, left_leg, count
	};

	enum class HumanArmPose : uint8
	{
		relaxed,
		weapon_ready
	};

	struct HumanPartDefinition
	{
		Mesh mesh;
		Mesh overlay_mesh;
		vec3 size = vec3(1.0f);
		vec3 pivot = vec3(0.0f);
		vec3 center_offset = vec3(0.0f);
		vec3 color = vec3(1.0f);
	};

	struct HumanDefinition
	{
		HumanPartDefinition parts[static_cast<size_t>(HumanPart::count)];
	};

	struct HumanComponent
	{
		// The attached point is the character's vertical center; feet are center - up * height / 2.
		HumanAppearance appearance;
		HumanArmPose arm_pose = HumanArmPose::relaxed;
		vec3 forward = vec3(0.0f, 1.0f, 0.0f);
		vec3 up = vec3(0.0f, 0.0f, 1.0f);
		vec3 desired_velocity = vec3(0.0f); // Owner-chunk coordinates, metres per second.
		vec2 head_rotation = vec2(0.0f); // Yaw and pitch relative to the torso.
		float height = 1.8f;
		float health = 100.0f;
		float movement_speed = 1.5f;
		float walk_phase = 0.0f;
		float walk_amplitude = 0.0f;
		float vertical_speed = 0.0f; // Along up; negative when falling.
		float gravity_acceleration = 9.8f; // Used when the owning body has no Planet component.
		bool grounded = false;
		bool follow_ground = true;
	};

	void loadHumanDefinition(HumanDefinition& definition);
	PointComponentSlotId createHumanSoldier(World& world, WorldBodyId world_body_id,
		const glm::dvec3& position, const vec3& up, const vec3& forward);
	void constructHumanComponent(void* destination, const void* init_data);
	void moveHumanComponent(void* destination, void* source);
	void destroyHumanComponent(void* data);
	void tickHumanComponent(PointComponentSlot& slot, World& world);
	void frameHumanComponent(PointComponentSlot& slot, World& world);
	void drawHumanAppearance(const HumanComponent& human, const mat4& world_matrix);
}
