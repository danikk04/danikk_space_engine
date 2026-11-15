#pragma once

#include <danikk_space_engine/component/point/point.h>
#include <danikk_space_engine/id.h>
#include <danikk_engine/mesh.h>

namespace danikk_space_engine
{
	enum class CharacterRenderType : uint8
	{
		minecraft_style,
		model_3d
	};

	struct CharacterAppearance
	{
		CharacterRenderType render_type = CharacterRenderType::minecraft_style;
		CharacterSkinId skin_id = 0;
	};

	enum class CharacterPart : uint8
	{
		head, torso, right_arm, left_arm, right_leg, left_leg, count
	};

	struct CharacterPartDefinition
	{
		Mesh mesh;
		vec3 size = vec3(1.0f);
		vec3 pivot = vec3(0.0f);
		vec3 center_offset = vec3(0.0f);
		vec3 color = vec3(1.0f);
	};

	struct CharacterDefinition
	{
		CharacterPartDefinition parts[static_cast<size_t>(CharacterPart::count)];
	};

	struct CharacterComponent
	{
		CharacterAppearance appearance;
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

	void loadCharacterDefinition(CharacterDefinition& definition);
	void constructCharacterComponent(void* destination, const void* init_data);
	void moveCharacterComponent(void* destination, void* source);
	void destroyCharacterComponent(void* data);
	void tickCharacterComponent(PointComponentSlot& slot, World& world);
	void frameCharacterComponent(PointComponentSlot& slot, World& world);
	void drawCharacterAppearance(const CharacterComponent& character, const mat4& world_matrix);
}
