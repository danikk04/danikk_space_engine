#include <cmath>
#include <danikk_space_engine/systems/world_body/system.h>

#include <danikk_space_engine/systems/telemetry/telemetry.h>
#include <danikk_space_engine/systems/physics/physics.h>
#include <danikk_space_engine/world.h>
#include <danikk_space_engine/systems/controller/controller.h>
#include <danikk_space_engine/systems/planet/system.h>
#include <danikk_space_engine/systems/physics/internal.h>

#include <danikk_space_engine/systems/world_body/world_body.h>
#include <danikk_space_engine/systems/build_object/system.h>
#include <danikk_space_engine/systems/build_part/system.h>
#include <danikk_space_engine/systems/environment/environment.h>

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
			if(!world.getControllerSystem().controls()) return static_cast<WorldBodyId>(-1);
			return world.getControllerSystem().dragged_world_body_id;
		}
	}

	/**
	 * @brief Applies sampled gravity and integrates position and rotation of eligible bodies.
		 * @param world World providing bodies and the gravity field.
	 * A non-positive target frame delay ends the stage. Planet bodies, the body
		 * held by Drag, and bodies without points or BuildParts are skipped. Linear
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

		for(WorldBodyId body_id : world.getWorldBodySystem().getUsedWorldBodyIds())
		{
			if(body_id == dragged_body_id) continue;
			WorldBody& body = world.getWorldBodySystem().getWorldBody(body_id);
			if(body.planet_id != static_cast<PlanetId>(-1)) continue;

			const BuildObjectId object_id = world.getBuildObjectSystem().findBuildObjectId(body_id);
			if(body.point_ids.size() == 0
				&& (object_id == static_cast<BuildObjectId>(-1)
					|| world.getBuildObjectSystem().getBuildObject(object_id).build_part_ids.size() == 0)) continue;
			const glm::dvec3 acceleration = world.getEnvironmentSystem().sampleGravityAt(body.position);
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
