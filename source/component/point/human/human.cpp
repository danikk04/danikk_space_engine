#include <cmath>

#include <danikk_space_engine/profiler.h>
#include <danikk_space_engine/component/point/human/human.h>
#include <danikk_space_engine/component/point/inventory.h>
#include <danikk_space_engine/component/world_body/planet/planet.h>
#include <danikk_space_engine/item.h>
#include <danikk_space_engine/world/world.h>

namespace danikk_space_engine
{
	namespace
	{
		constexpr float minimum_ground_normal_dot = 0.2f;

		/**
		 * @brief Returns planet.
		 */
		const PlanetComponent* getPlanet(const WorldData& data, const WorldBody& body)
		{
			for(WorldBodyComponentSlotId component_id : body.component_slot_ids)
			{
				if(!data.isWorldBodyComponentSlotActive(component_id)) continue;
				const WorldBodyComponentSlot& slot = data.getWorldBodyComponentSlot(component_id);
				if(slot.type_id == WorldBodyComponentTypeId::planet)
					return static_cast<const PlanetComponent*>(slot.getData());
			}
			return nullptr;
		}

		/**
		 * @brief Finds planet ground position.
		 */
		bool findPlanetGroundPosition(const Chunk& chunk, const WorldBody& body,
			const PlanetComponent& planet, const vec3& up, float human_height,
			const vec3& proposed, vec3& supported, float step_height, float maximum_drop)
		{
			const double capsule_radius = human_height * 0.14;
			const double half_axis = human_height * 0.5 - capsule_radius;
			const glm::dvec3 bottom_center = chunk.position
				+ glm::dvec3(proposed - up * static_cast<float>(half_axis));
			const auto densityAtOffset = [&](double offset)
			{
				return samplePlanetScalarField(bottom_center + glm::dvec3(up) * offset,
					planet.radius, planet.noise_seed, planet.terrain_edits, planet.core_radius);
			};
			double upper = step_height;
			double lower = -maximum_drop;
			if(densityAtOffset(upper) > -capsule_radius || densityAtOffset(lower) < -capsule_radius) return false;
			for(uint8 iteration = 0; iteration < 8; iteration++)
			{
				const double middle = (upper + lower) * 0.5;
				if(densityAtOffset(middle) < -capsule_radius) upper = middle;
				else lower = middle;
			}
			const glm::dvec3 contact_position = bottom_center + glm::dvec3(up) * ((upper + lower) * 0.5);
			constexpr double gradient_step = planet_voxel_size * 0.25;
			glm::dvec3 gradient(0.0);
			for(uint8 axis = 0; axis < 3; axis++)
			{
				glm::dvec3 offset(0.0);
				offset[axis] = gradient_step;
				gradient[axis] = (samplePlanetScalarField(contact_position + offset,
					planet.radius, planet.noise_seed, planet.terrain_edits, planet.core_radius)
					- samplePlanetScalarField(contact_position - offset,
						planet.radius, planet.noise_seed, planet.terrain_edits, planet.core_radius))
					/ (2.0 * gradient_step);
			}
			const double gradient_length = glm::length(gradient);
			if(gradient_length <= 1e-8 || glm::dot(-gradient / gradient_length, glm::dvec3(up))
				< minimum_ground_normal_dot) return false;
			const glm::dvec3 supported_bottom = contact_position + glm::dvec3(up) * 0.02;
			supported = vec3(supported_bottom - chunk.position) + up * static_cast<float>(half_axis);
			(void)body;
			return true;
		}

		// Derives the feet from the center point, then probes for support on the current body.
		// Grounded movement uses step limits; falling checks full tick travel to avoid tunnelling.

		/**
		 * @brief Finds ground position.
		 */
		bool findGroundPosition(World& world, const Chunk& chunk, const WorldBody& body,
			const vec3& up, float human_height, const vec3& proposed, vec3& supported,
			float step_height = 0.5f, float maximum_drop = 0.75f)
		{
			if(const PlanetComponent* planet = getPlanet(world.data, body))
				return findPlanetGroundPosition(chunk, body, *planet, up, human_height,
					proposed, supported, step_height, maximum_drop);

			const vec3 proposed_foot = proposed - up * (human_height * 0.5f);
			const glm::dvec3 foot = body.position
				+ body.rotation * (chunk.position + glm::dvec3(proposed_foot));
			const vec3 world_up = vec3(body.rotation * glm::dvec3(up));
			const RaycastHit hit = world.raycast(foot + glm::dvec3(world_up) * static_cast<double>(step_height),
				-world_up, step_height + maximum_drop);
			if(!hit.hit || !world.data.isChunkActive(hit.face.chunk_id)
				|| world.data.getChunk(hit.face.chunk_id).world_body_id != chunk.world_body_id
				|| glm::dot(hit.normal, world_up) < minimum_ground_normal_dot) return false;
			const vec3 supported_foot = vec3(glm::conjugate(body.rotation)
				* (hit.position - body.position) - chunk.position) + up * 0.02f;
			supported = supported_foot + up * (human_height * 0.5f);
			return true;
		}

		// Keeps horizontal steps on supported slopes. Once support under the current feet is lost,
		// integrates vertical speed and sweeps the entire downward travel before accepting a landing.

		/**
		 * @brief Moves with ground.
		 */
		void moveWithGround(World& world, const Chunk& chunk, const WorldBody& body,
			HumanComponent& human, Point& point, const vec3& velocity, float gravity, float dt)
		{
			if(!human.follow_ground)
			{
				point.position += velocity * dt;
				human.vertical_speed = 0.0f;
				human.grounded = false;
				return;
			}
			vec3 support;
			if(human.vertical_speed <= 0.0f
				&& findGroundPosition(world, chunk, body, human.up, human.height, point.position, support)
				&& std::abs(glm::dot(support - point.position, human.up)) <= 0.08f)
			{
				point.position = support;
				if(findGroundPosition(world, chunk, body, human.up, human.height,
					point.position + velocity * dt, support))
					point.position = support;
				human.vertical_speed = 0.0f;
				human.grounded = true;
				return;
			}

			human.grounded = false;
			const float vertical_displacement = human.vertical_speed * dt - 0.5f * gravity * dt * dt;
			human.vertical_speed -= gravity * dt;
			if(vertical_displacement <= 0.0f
				&& findGroundPosition(world, chunk, body, human.up, human.height, point.position, support,
					0.0f, -vertical_displacement + 0.02f))
			{
				point.position = support;
				human.vertical_speed = 0.0f;
				human.grounded = true;
			}
			else
			{
				point.position += human.up * vertical_displacement;
			}
		}

		// Draws equipped MG-42 items attached to the soldier's weapon-ready pose. The OBJ's X/Y/Z
		// axes map to character forward/up/right; the attachment offset is expressed in body-height units.

		/**
		 * @brief Draws equipped weapons.
		 */
		void drawEquippedWeapons(const PointComponentSlot& human_slot, const HumanComponent& human,
			const mat4& body_matrix, World& world)
		{
			if(human.arm_pose != HumanArmPose::weapon_ready) return;
			const Chunk& chunk = world.data.getChunk(human_slot.chunk_id);
			for(PointComponentSlotId component_slot_id : chunk.point_component_slot_ids)
			{
				if(!world.data.isPointComponentSlotActive(component_slot_id)) continue;
				const PointComponentSlot& component_slot = world.data.getPointComponentSlot(component_slot_id);
				if(component_slot.point_id != human_slot.point_id
					|| component_slot.type_id != PointComponentTypeId::inventory) continue;
				const InventoryComponent& inventory = *static_cast<const InventoryComponent*>(component_slot.getData());
				for(const InventorySlot& inventory_slot : inventory.slots)
				{
					if(!inventory_slot.is_equipped || inventory_slot.isEmpty()) continue;
					if(inventory_slot.item_definition_id != item_definition_ids::mg42) continue;

					mat4 weapon_orientation(1.0f);
					weapon_orientation[0] = vec4(0, 1, 0, 0);
					weapon_orientation[1] = vec4(0, 0, 1, 0);
					weapon_orientation[2] = vec4(1, 0, 0, 0);
					const vec3 shoulder_position = vec3(0.1875f, 0.0f, 0.25f) * human.height;
					const vec3 attachment_offset = (vec3(0.11f, 0.37f, 0.22f)
						- vec3(0.1875f, 0.0f, 0.25f)) * human.height;
					const mat4 weapon_pitch = glm::rotate(mat4(1.0f),
						glm::clamp(human.head_rotation.y, -0.8f, 0.8f), vec3(1, 0, 0));
					const mat4 weapon_matrix = body_matrix
						* glm::translate(mat4(1.0f), shoulder_position)
						* weapon_pitch
						* glm::translate(mat4(1.0f), attachment_offset)
						* weapon_orientation
						* glm::scale(mat4(1.0f), vec3(0.25f));
					drawItem(inventory_slot.item_definition_id, weapon_matrix);
				}
				return;
			}
		}
	}

	/**
	 * @brief Constructs human component.
	 */
	void constructHumanComponent(void* destination, const void* init_data)
	{
		if(destination == nullptr) formatFatalError("Human construction received null destination");
		new (destination) HumanComponent(init_data == nullptr
			? HumanComponent() : *static_cast<const HumanComponent*>(init_data));
	}

	/**
	 * @brief Moves human component.
	 */
	void moveHumanComponent(void* destination, void* source)
	{
		if(destination == nullptr || source == nullptr) formatFatalError("Human move received null memory");
		new (destination) HumanComponent(*static_cast<HumanComponent*>(source));
		static_cast<HumanComponent*>(source)->~HumanComponent();
	}

	/**
	 * @brief Removes and releases human component.
	 */
	void destroyHumanComponent(void* data)
	{
		if(data == nullptr) formatFatalError("Human destruction received null data");
		static_cast<HumanComponent*>(data)->~HumanComponent();
	}

	// Consumes a chunk-local movement command, updates the point and advances walking by distance.
	// Planet ownership supplies radial up and gravity. Lost support triggers swept vertical falling;
	// walking animation follows actual horizontal movement, not the fall distance.

	/**
	 * @brief Runs one simulation tick for human component.
	 */
	void tickHumanComponent(PointComponentSlot& slot, World& world)
	{
		/**
		 * @brief Disables copying to ensure a measured scope is recorded once.
		 */
		Watcher watcher("HumanComponent::tick");
		if(!world.data.isPointActive(slot.point_id) || !world.data.isChunkActive(slot.chunk_id))
			formatFatalError("Human references an invalid point or chunk");
		HumanComponent& human = *static_cast<HumanComponent*>(slot.getData());
		Point& point = world.data.getPoint(slot.point_id);
		const Chunk& chunk = world.data.getChunk(slot.chunk_id);
		const WorldBody& body = world.data.getWorldBody(chunk.world_body_id);
		const glm::dvec3 world_position = world.getPointWorldPosition(slot.chunk_id, slot.point_id);
		const glm::dvec3 gravity_world = world.environment.sampleGravityAt(world, world_position);
		const double gravity_magnitude = glm::length(gravity_world);
		const float gravity = static_cast<float>(gravity_magnitude);
		if(gravity_magnitude > 1e-8)
		{
			human.up = vec3(glm::conjugate(body.rotation) * (-gravity_world / gravity_magnitude));
		}
		if(glm::length(human.up) < 1e-6f) human.up = vec3(0, 0, 1);
		human.up = glm::normalize(human.up);
		const float dt = glm::max(getTargetFrameDelay(), 0.0f);
		vec3 velocity = human.health > 0.0f ? human.desired_velocity : vec3(0.0f);
		human.desired_velocity = vec3(0.0f);
		velocity -= human.up * glm::dot(velocity, human.up);
		const float speed = glm::length(velocity);
		const float max_speed = glm::max(human.movement_speed, 0.0f);
		if(speed > max_speed && speed > 0.0f) velocity *= max_speed / speed;
		const vec3 old_position = point.position;
		moveWithGround(world, chunk, body, human, point, velocity, gravity, dt);
		vec3 displacement = point.position - old_position;
		displacement -= human.up * glm::dot(displacement, human.up);
		const float distance = glm::length(displacement);
		human.forward -= human.up * glm::dot(human.forward, human.up);

		if(glm::length(human.forward) < 1e-6f)
		{
			const vec3 reference = std::abs(human.up.y) < 0.9f ? vec3(0, 1, 0) : vec3(1, 0, 0);
			human.forward = reference - human.up * glm::dot(reference, human.up);
		}
		human.forward = glm::normalize(human.forward);
		if(distance > 1e-6f)
		{
			const vec3 heading = displacement / distance;
			const float angle = std::atan2(glm::dot(human.up, glm::cross(human.forward, heading)),
				glm::clamp(glm::dot(human.forward, heading), -1.0f, 1.0f));
			human.forward = glm::angleAxis(glm::clamp(angle, -dt * 4.0f, dt * 4.0f), human.up)
				* human.forward;
		}
		human.walk_phase = std::fmod(human.walk_phase + distance * 6.0f, 6.283185307f);
		const float actual_speed = dt > 0.0f ? distance / dt : 0.0f;
		const float target_amplitude = max_speed > 0.0f ? glm::min(actual_speed / max_speed, 1.0f) * 0.6f : 0.0f;
		human.walk_amplitude = glm::mix(human.walk_amplitude, target_amplitude, glm::min(dt * 10.0f, 1.0f));
	}

	// Builds the common human transform and dispatches rendering through its appearance type.
	// World translation is camera-relative in double before conversion to the float render matrix.

	/**
	 * @brief Processes one frame for human component.
	 */
	void frameHumanComponent(PointComponentSlot& slot, World& world)
	{
		if(!world.data.isPointActive(slot.point_id) || !world.data.isChunkActive(slot.chunk_id)) return;
		const HumanComponent& human = *static_cast<const HumanComponent*>(slot.getData());
		const Chunk& chunk = world.data.getChunk(slot.chunk_id);
		const WorldBody& body = world.data.getWorldBody(chunk.world_body_id);
		const vec3 up = vec3(body.rotation * glm::dvec3(human.up));
		const vec3 forward = vec3(body.rotation * glm::dvec3(human.forward));
		const vec3 right = glm::normalize(glm::cross(forward, up));
		mat4 orientation(1.0f);
		orientation[0] = vec4(right, 0);
		orientation[1] = vec4(forward, 0);
		orientation[2] = vec4(up, 0);
		const glm::dvec3 position = world.getPointWorldPosition(slot.chunk_id, slot.point_id);
		const mat4 body_matrix = glm::translate(mat4(1.0f), vec3(position - world.getCameraWorldPosition()))
			* orientation;
		const mat4 feet_matrix = body_matrix
			* glm::translate(mat4(1.0f), vec3(0.0f, 0.0f, -0.5f))
			* glm::scale(mat4(1.0f), vec3(human.height));
		drawHumanAppearance(human, feet_matrix);
		drawEquippedWeapons(slot, human, body_matrix, world);
	}
}
