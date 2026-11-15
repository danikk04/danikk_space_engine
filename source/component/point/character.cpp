#include <cmath>

#include <danikk_space_engine/profiler.h>
#include <danikk_space_engine/component/point/character.h>
#include <danikk_space_engine/component/world_body/planet/planet.h>
#include <danikk_space_engine/world/world.h>

namespace danikk_space_engine
{
	namespace
	{
		// Probes below the proposed foot position. Only support on the current body is accepted;
		// Grounded movement uses step limits; falling supplies the full tick travel to avoid tunnelling.
		bool findGroundPosition(World& world, const Chunk& chunk, const WorldBody& body,
			const vec3& up, const vec3& proposed, vec3& supported,
			float step_height = 0.5f, float maximum_drop = 0.75f)
		{
			const glm::dvec3 foot = body.position
				+ body.rotation * (chunk.position + glm::dvec3(proposed));
			const vec3 world_up = vec3(body.rotation * glm::dvec3(up));
			const RaycastHit hit = world.raycast(foot + glm::dvec3(world_up) * static_cast<double>(step_height),
				-world_up, step_height + maximum_drop);
			if(!hit.hit || !world.data.isChunkActive(hit.face.chunk_id)
				|| world.data.getChunk(hit.face.chunk_id).world_body_id != chunk.world_body_id
				|| glm::dot(hit.normal, world_up) < 0.5f) return false;
			supported = vec3(glm::conjugate(body.rotation) * (hit.position - body.position) - chunk.position)
				+ up * 0.02f;
			return true;
		}

		// Keeps horizontal steps on supported slopes. Once support under the current feet is lost,
		// integrates vertical speed and sweeps the entire downward travel before accepting a landing.
		void moveWithGround(World& world, const Chunk& chunk, const WorldBody& body,
			CharacterComponent& character, Point& point, const vec3& velocity, float gravity, float dt)
		{
			if(!character.follow_ground)
			{
				point.position += velocity * dt;
				character.vertical_speed = 0.0f;
				character.grounded = false;
				return;
			}
			vec3 support;
			if(character.vertical_speed <= 0.0f
				&& findGroundPosition(world, chunk, body, character.up, point.position, support)
				&& std::abs(glm::dot(support - point.position, character.up)) <= 0.08f)
			{
				point.position = support;
				if(findGroundPosition(world, chunk, body, character.up, point.position + velocity * dt, support))
					point.position = support;
				character.vertical_speed = 0.0f;
				character.grounded = true;
				return;
			}

			character.grounded = false;
			const float vertical_displacement = character.vertical_speed * dt - 0.5f * gravity * dt * dt;
			character.vertical_speed -= gravity * dt;
			if(vertical_displacement <= 0.0f
				&& findGroundPosition(world, chunk, body, character.up, point.position, support,
					0.0f, -vertical_displacement + 0.02f))
			{
				point.position = support;
				character.vertical_speed = 0.0f;
				character.grounded = true;
			}
			else
			{
				point.position += character.up * vertical_displacement;
			}
		}
	}

	void constructCharacterComponent(void* destination, const void* init_data)
	{
		if(destination == nullptr) formatFatalError("Character construction received null destination");
		new (destination) CharacterComponent(init_data == nullptr
			? CharacterComponent() : *static_cast<const CharacterComponent*>(init_data));
	}

	void moveCharacterComponent(void* destination, void* source)
	{
		if(destination == nullptr || source == nullptr) formatFatalError("Character move received null memory");
		new (destination) CharacterComponent(*static_cast<CharacterComponent*>(source));
		static_cast<CharacterComponent*>(source)->~CharacterComponent();
	}

	void destroyCharacterComponent(void* data)
	{
		if(data == nullptr) formatFatalError("Character destruction received null data");
		static_cast<CharacterComponent*>(data)->~CharacterComponent();
	}

	// Consumes a chunk-local movement command, updates the point and advances walking by distance.
	// Planet ownership supplies radial up and gravity. Lost support triggers swept vertical falling;
	// walking animation follows actual horizontal movement, not the fall distance.
	void tickCharacterComponent(PointComponentSlot& slot, World& world)
	{
		Watcher watcher("CharacterComponent::tick");
		if(!world.data.isPointActive(slot.point_id) || !world.data.isChunkActive(slot.chunk_id))
			formatFatalError("Character references an invalid point or chunk");
		CharacterComponent& character = *static_cast<CharacterComponent*>(slot.getData());
		Point& point = world.data.getPoint(slot.point_id);
		const Chunk& chunk = world.data.getChunk(slot.chunk_id);
		const WorldBody& body = world.data.getWorldBody(chunk.world_body_id);
		float gravity = glm::max(character.gravity_acceleration, 0.0f);
		for(WorldBodyComponentSlotId component_id : body.component_slot_ids)
		{
			if(!world.data.isWorldBodyComponentSlotActive(component_id)) continue;
			if(world.data.getWorldBodyComponentSlot(component_id).type_id != WorldBodyComponentTypeId::planet) continue;
			const PlanetComponent& planet = *static_cast<const PlanetComponent*>(
				world.data.getWorldBodyComponentSlot(component_id).getData());
			gravity = static_cast<float>(glm::max(planet.surface_gravity, 0.0));
			const glm::dvec3 radial = chunk.position + glm::dvec3(point.position);
			if(glm::length(radial) > 1e-6) character.up = vec3(glm::normalize(radial));
			break;
		}
		if(glm::length(character.up) < 1e-6f) character.up = vec3(0, 0, 1);
		character.up = glm::normalize(character.up);
		const float dt = glm::max(getTargetFrameDelay(), 0.0f);
		vec3 velocity = character.health > 0.0f ? character.desired_velocity : vec3(0.0f);
		character.desired_velocity = vec3(0.0f);
		velocity -= character.up * glm::dot(velocity, character.up);
		const float speed = glm::length(velocity);
		const float max_speed = glm::max(character.movement_speed, 0.0f);
		if(speed > max_speed && speed > 0.0f) velocity *= max_speed / speed;
		const vec3 old_position = point.position;
		moveWithGround(world, chunk, body, character, point, velocity, gravity, dt);
		vec3 displacement = point.position - old_position;
		displacement -= character.up * glm::dot(displacement, character.up);
		const float distance = glm::length(displacement);
		character.forward -= character.up * glm::dot(character.forward, character.up);
		if(glm::length(character.forward) < 1e-6f)
		{
			const vec3 reference = std::abs(character.up.y) < 0.9f ? vec3(0, 1, 0) : vec3(1, 0, 0);
			character.forward = reference - character.up * glm::dot(reference, character.up);
		}
		character.forward = glm::normalize(character.forward);
		if(distance > 1e-6f)
		{
			const vec3 heading = displacement / distance;
			const float angle = std::atan2(glm::dot(character.up, glm::cross(character.forward, heading)),
				glm::clamp(glm::dot(character.forward, heading), -1.0f, 1.0f));
			character.forward = glm::angleAxis(glm::clamp(angle, -dt * 4.0f, dt * 4.0f), character.up)
				* character.forward;
		}
		character.walk_phase = std::fmod(character.walk_phase + distance * 6.0f, 6.283185307f);
		const float actual_speed = dt > 0.0f ? distance / dt : 0.0f;
		const float target_amplitude = max_speed > 0.0f ? glm::min(actual_speed / max_speed, 1.0f) * 0.6f : 0.0f;
		character.walk_amplitude = glm::mix(character.walk_amplitude, target_amplitude, glm::min(dt * 10.0f, 1.0f));
	}

	// Builds the common character transform and dispatches rendering through its appearance type.
	// World translation is camera-relative in double before conversion to the float render matrix.
	void frameCharacterComponent(PointComponentSlot& slot, World& world)
	{
		if(!world.data.isPointActive(slot.point_id) || !world.data.isChunkActive(slot.chunk_id)) return;
		const CharacterComponent& character = *static_cast<const CharacterComponent*>(slot.getData());
		const Chunk& chunk = world.data.getChunk(slot.chunk_id);
		const WorldBody& body = world.data.getWorldBody(chunk.world_body_id);
		const vec3 up = vec3(body.rotation * glm::dvec3(character.up));
		const vec3 forward = vec3(body.rotation * glm::dvec3(character.forward));
		const vec3 right = glm::normalize(glm::cross(forward, up));
		mat4 orientation(1.0f);
		orientation[0] = vec4(right, 0);
		orientation[1] = vec4(forward, 0);
		orientation[2] = vec4(up, 0);
		const glm::dvec3 position = world.getPointWorldPosition(slot.chunk_id, slot.point_id);
		const mat4 base = glm::translate(mat4(1.0f), vec3(position - world.getCameraWorldPosition()))
			* orientation * glm::scale(mat4(1.0f), vec3(character.height));
		drawCharacterAppearance(character, base);
	}
}
