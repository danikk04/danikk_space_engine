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

		const WorldBodyId dragged_body_id = getDraggedWorldBodyId(world);
		for(WorldBodyId body_id : world.data.getUsedWorldBodyIds())
		{
			if(body_id == dragged_body_id) continue;
			bool is_planet = false;
			WorldBody& body = world.data.getWorldBody(body_id);
			for(WorldBodyComponentSlotId slot_id : body.component_slot_ids)
			{
				if(!world.data.isWorldBodyComponentSlotActive(slot_id)) continue;
				if(world.data.getWorldBodyComponentSlot(slot_id).type_id == WorldBodyComponentTypeId::planet)
					is_planet = true;
			}
			if(is_planet) continue;

			if(!collision_internal::hasLoadedChunk(world, body)) continue;
			const glm::dvec3 acceleration = world.environment.sampleGravityAt(world, body.position);
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
