#include <cmath>

#include <danikk_space_engine/systems/telemetry/telemetry.h>
#include <danikk_space_engine/systems/physics/physics.h>
#include <danikk_space_engine/world.h>
#include <danikk_space_engine/systems/controller/controller.h>
#include <danikk_space_engine/systems/planet/system.h>
#include "internal.h"

namespace danikk_space_engine
{
	namespace
	{

		/**
		 * @brief Reads the body currently controlled by the active local Drag tool.
		 * @param world World controlled by the singleton ControllerSystem.
		 * @return Dragged body ID, or the invalid WorldBodyId sentinel when the
		 *         no ControllerSystem is bound to this world.
		 */
		WorldBodyId getDraggedWorldBodyId(const World& world)
		{
			if(!world.controller.controls()) return static_cast<WorldBodyId>(-1);
			return world.controller.dragged_world_body_id;
		}
	}

	/**
	 * @brief Applies sampled gravity and integrates position and rotation of eligible bodies.
		 * @param world World providing bodies and the gravity field.
	 * A non-positive target frame delay ends the stage. Planet bodies, the body
		 * held by Drag, and bodies without points are skipped. Linear
	 * acceleration and position use semi-implicit Euler integration; rotation is
	 * advanced from angular velocity using an axis-angle delta and normalized.
	 */
	void PhysicsSystem::integrateMotion()
	{
		/**
		 * @brief Measures the duration of the enclosing profiler scope.
		 */
		DSE_WATCH();
		const float delta_time = danikk_engine::getTargetFrameDelay();
		if(delta_time <= 0.0f) return;

		const WorldBodyId dragged_body_id = getDraggedWorldBodyId(world);

		for(WorldBodyId body_id : world.world_body_data.getUsedWorldBodyIds())
		{
			if(body_id == dragged_body_id) continue;
			WorldBody& body = world.world_body_data.getWorldBody(body_id);
			if(world.planets.isPlanet(body_id)) continue;

			if(!collision_internal::hasSpatialPoints(body)) continue;
			const glm::dvec3 acceleration = world.environment.sampleGravityAt(body.position);
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
