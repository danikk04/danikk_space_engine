#include <cmath>

#include <danikk_space_engine/profiler.h>
#include <danikk_space_engine/world_physics.h>
#include <danikk_space_engine/world/world.h>
#include <danikk_space_engine/component/world_body/planet/planet.h>
#include "internal.h"

namespace danikk_space_engine
{
	namespace
	{
		struct PlanetCollisionTarget
		{
			WorldBodyId body_id;
			const PlanetComponent* planet;
		};

		WorldBodyId getDraggedWorldBodyId(const World& world)
		{
			if(world.local_player_controller_component_slot_id >= world.data.getPointComponentSlotCount()
				|| !world.data.isPointComponentSlotActive(world.local_player_controller_component_slot_id))
			{
				return static_cast<WorldBodyId>(-1);
			}
			const PointComponentSlot& slot = world.data.getPointComponentSlot(
				world.local_player_controller_component_slot_id);
			return static_cast<const LocalPlayerControllerComponent*>(slot.getData())->dragged_world_body_id;
		}
	}

	// Applies planetary gravity, integrates linear velocity, then updates body rotation.
	void WorldPhysics::integrateMotion(World& world)
	{
		Watcher watcher("World::body motion");
		const float delta_time = danikk_engine::getTargetFrameDelay();
		if(delta_time <= 0.0f) return;

		DynamicArray<PlanetCollisionTarget> planets;
		for(WorldBodyId body_id : world.data.getUsedWorldBodyIds())
		{
			const WorldBody& body = world.data.getWorldBody(body_id);
			for(WorldBodyComponentSlotId slot_id : body.component_slot_ids)
			{
				if(!world.data.isWorldBodyComponentSlotActive(slot_id)) continue;
				const WorldBodyComponentSlot& slot = world.data.getWorldBodyComponentSlot(slot_id);
				if(slot.type_id != WorldBodyComponentTypeId::planet) continue;
				planets.push(PlanetCollisionTarget{
					body_id,
					static_cast<const PlanetComponent*>(slot.getData())});
			}
		}

		const WorldBodyId dragged_body_id = getDraggedWorldBodyId(world);
		for(WorldBodyId body_id : world.data.getUsedWorldBodyIds())
		{
			if(body_id == dragged_body_id) continue;
			bool is_planet = false;
			for(const PlanetCollisionTarget& target : planets)
			{
				if(target.body_id == body_id)
				{
					is_planet = true;
					break;
				}
			}
			if(is_planet) continue;

			WorldBody& body = world.data.getWorldBody(body_id);
			if(!collision_internal::hasLoadedChunk(world, body)) continue;
			glm::dvec3 acceleration(0.0);
			for(const PlanetCollisionTarget& target : planets)
			{
				if(target.body_id == body_id || target.planet->surface_gravity <= 0.0
					|| target.planet->radius <= 0.0) continue;

				const glm::dvec3 toward_planet = world.data.getWorldBody(target.body_id).position - body.position;
				const double distance = glm::length(toward_planet);
				if(distance <= 0.000001) continue;
				const double radius = target.planet->radius;
				const double distance_ratio = radius / distance;
				const double gravity = distance > radius
					? target.planet->surface_gravity * distance_ratio * distance_ratio
					: target.planet->surface_gravity * distance_ratio;
				acceleration += toward_planet / distance * gravity;
			}

			body.linear_velocity += acceleration * static_cast<double>(delta_time);
			body.position += body.linear_velocity * static_cast<double>(delta_time);

			const double angular_speed = glm::length(body.angular_velocity);
			if(angular_speed > 0.000001)
			{
				const glm::dquat rotation_delta = glm::angleAxis(
					angular_speed * static_cast<double>(delta_time),
					body.angular_velocity / angular_speed);
				body.rotation = glm::normalize(rotation_delta * body.rotation);
			}
		}
	}
}
