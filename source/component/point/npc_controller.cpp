#include <cmath>

#include <danikk_space_engine/profiler.h>
#include <danikk_space_engine/component/point/npc_controller.h>
#include <danikk_space_engine/component/point/character.h>
#include <danikk_space_engine/world/world.h>

namespace danikk_space_engine
{
	namespace
	{
		float randomUnit(NpcControllerComponent& controller)
		{
			uint32& state = controller.random_state;
			state ^= state << 13;
			state ^= state >> 17;
			state ^= state << 5;
			return static_cast<float>(state >> 8) / 16777216.0f;
		}

		// Alternates idle pauses and short trips around a fixed local centre. A timeout releases
		// unreachable targets so unloaded terrain or an edge cannot leave an NPC walking forever.
		void updateWandering(NpcControllerComponent& controller, const CharacterComponent& character,
			const vec3& position, float dt)
		{
			if(!controller.wander_enabled) return;
			if(!controller.wander_initialized)
			{
				controller.wander_center = position;
				controller.wander_initialized = true;
				controller.wander_timer = 0.5f + randomUnit(controller);
			}
			controller.wander_timer = glm::max(0.0f, controller.wander_timer - dt);
			if(controller.has_movement_target)
			{
				if(controller.wander_timer == 0.0f)
				{
					controller.has_movement_target = false;
					controller.wander_timer = 1.0f + randomUnit(controller) * 2.0f;
				}
				return;
			}
			if(controller.wander_timer > 0.0f) return;
			const float angle = randomUnit(controller) * 6.283185307f;
			const float radius = std::sqrt(randomUnit(controller)) * glm::max(controller.wander_radius, 0.0f);
			const vec3 reference = std::abs(character.up.y) < 0.9f ? vec3(0, 1, 0) : vec3(1, 0, 0);
			const vec3 forward = glm::normalize(reference - character.up * glm::dot(reference, character.up));
			const vec3 right = glm::cross(forward, character.up);
			controller.target_position = controller.wander_center
				+ (forward * std::cos(angle) + right * std::sin(angle)) * radius;
			controller.has_movement_target = true;
			controller.wander_timer = 3.0f + radius * 2.0f / glm::max(character.movement_speed, 0.1f);
		}

		void updateRandomLook(NpcControllerComponent& controller, CharacterComponent& character, float dt)
		{
			controller.look_timer -= dt;
			if(controller.look_timer <= 0.0f)
			{
				controller.look_timer = 0.7f + randomUnit(controller) * 2.0f;
				controller.look_target = vec2((randomUnit(controller) - 0.5f) * 1.8f,
					(randomUnit(controller) - 0.5f) * 0.6f);
			}
			character.head_rotation = glm::mix(character.head_rotation,
				controller.look_target, 1.0f - std::exp(-dt * 4.0f));
		}
	}

	void constructNpcControllerComponent(void* destination, const void* init_data)
	{
		if(destination == nullptr) formatFatalError("NPC controller construction received null destination");
		new (destination) NpcControllerComponent(init_data == nullptr
			? NpcControllerComponent() : *static_cast<const NpcControllerComponent*>(init_data));
	}

	void moveNpcControllerComponent(void* destination, void* source)
	{
		if(destination == nullptr || source == nullptr) formatFatalError("NPC controller move received null memory");
		new (destination) NpcControllerComponent(*static_cast<NpcControllerComponent*>(source));
		static_cast<NpcControllerComponent*>(source)->~NpcControllerComponent();
	}

	void destroyNpcControllerComponent(void* data)
	{
		if(data == nullptr) formatFatalError("NPC controller destruction received null data");
		static_cast<NpcControllerComponent*>(data)->~NpcControllerComponent();
	}

	// Chooses random local trips or follows an explicit target, then writes the next movement command.
	// Random head turns replace camera tracking by default; the character handles support checks.
	void tickNpcControllerComponent(PointComponentSlot& slot, World& world)
	{
		Watcher watcher("NpcControllerComponent::tick");
		NpcControllerComponent& controller = *static_cast<NpcControllerComponent*>(slot.getData());
		if(!world.data.isPointComponentSlotActive(controller.character_slot_id)) return;
		PointComponentSlot& character_slot = world.data.getPointComponentSlot(controller.character_slot_id);
		if(character_slot.type_id != PointComponentTypeId::character
			|| character_slot.point_id != slot.point_id || character_slot.chunk_id != slot.chunk_id)
			formatFatalError("NPC controller and character must belong to the same point and chunk");
		if(!world.data.isPointActive(slot.point_id) || !world.data.isChunkActive(slot.chunk_id)) return;
		CharacterComponent& character = *static_cast<CharacterComponent*>(character_slot.getData());
		character.desired_velocity = vec3(0.0f);
		if(!controller.enabled || character.health <= 0.0f) return;
		if(controller.random_state == 0)
		{
			controller.random_state = slot.point_id * 747796405u + controller.character_slot_id * 2891336453u + 1u;
			if(controller.random_state == 0) controller.random_state = 1;
		}
		const float dt = glm::max(getTargetFrameDelay(), 0.0f);
		updateWandering(controller, character, world.data.getPoint(slot.point_id).position, dt);

		if(controller.has_movement_target)
		{
			vec3 offset = controller.target_position - world.data.getPoint(slot.point_id).position;
			offset -= character.up * glm::dot(offset, character.up);
			const float distance = glm::length(offset);
			if(distance <= 0.02f)
			{
				controller.has_movement_target = false;
				if(controller.wander_enabled) controller.wander_timer = 1.0f + randomUnit(controller) * 2.0f;
			}
			else if(dt > 0.0f)
				character.desired_velocity = offset / distance
					* glm::min(glm::max(character.movement_speed, 0.0f), distance / dt);
		}

		if(controller.look_at_camera && world.data.isPointComponentSlotActive(world.camera_component_slot_id))
		{
			const Chunk& chunk = world.data.getChunk(slot.chunk_id);
			const WorldBody& body = world.data.getWorldBody(chunk.world_body_id);
			const glm::dvec3 head_position = world.getPointWorldPosition(slot.chunk_id, slot.point_id)
				+ body.rotation * glm::dvec3(character.up * (character.height * 0.875f));
			const vec3 direction = vec3(glm::conjugate(body.rotation)
				* (world.getCameraWorldPosition() - head_position));
			const vec3 right = glm::normalize(glm::cross(character.forward, character.up));
			const float front = glm::dot(direction, character.forward);
			const float side = glm::dot(direction, right);
			const float vertical = glm::dot(direction, character.up);
			character.head_rotation.x = glm::clamp(std::atan2(-side, front), -1.2f, 1.2f);
			character.head_rotation.y = glm::clamp(std::atan2(vertical, std::sqrt(front * front + side * side)), -0.8f, 0.8f);
		}
		else
		{
			updateRandomLook(controller, character, dt);
		}
	}
}
