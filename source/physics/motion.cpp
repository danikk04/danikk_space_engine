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

		/**
		 * @brief Reads the body currently controlled by the active local Drag tool.
		 * @param world World containing the controller component slot.
		 * @return Dragged body ID, or the invalid WorldBodyId sentinel when the
		 *         controller slot is unavailable or inactive.
		 */
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

	/**
	 * @brief Applies sampled gravity and integrates position and rotation of eligible bodies.
	 * @param world World providing bodies, loaded-chunk state, and the gravity field.
	 * A non-positive target frame delay ends the stage. Planet bodies, the body
	 * held by Drag, and bodies without an active chunk are skipped. Linear
	 * acceleration and position use semi-implicit Euler integration; rotation is
	 * advanced from angular velocity using an axis-angle delta and normalized.
	 */
	void WorldPhysics::integrateMotion(World& world)
	{
		/**
		 * @brief Measures the duration of the enclosing profiler scope.
		 */
		DSE_WATCH();
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
