#include <cmath>
#include <danikk_space_engine/systems/world_body/system.h>

#include <danikk_space_engine/systems/telemetry/telemetry.h>
#include <danikk_space_engine/systems/physics/physics.h>
#include <danikk_space_engine/systems/controller/controller.h>
#include <danikk_space_engine/systems/physics/internal.h>

#include <danikk_space_engine/systems/world_body/world_body.h>

namespace danikk_space_engine
{
	using namespace collision_internal;

	/**
	 * @brief Builds convex mesh proxies and resolves body pairs in four sequential passes.
	 * @param world World whose loaded mesh bodies may be moved and have velocities changed.
	 * Planet bodies and bodies without BuildParts are excluded. Each unordered
	 * body pair is broadphase-tested by its aggregate radius, then each part pair
	 * is tested by a bounding sphere and SAT. The Drag-held body has zero inverse
	 * mass and inertia, so the other body receives the pair correction and impulse.
	 */
	void PhysicsSystem::resolveBodyCollisions()
	{
		/**
		 * @brief Measures the duration of the enclosing profiler scope.
		 */
		DSE_WATCH();
		WorldBodyId dragged_id = static_cast<WorldBodyId>(-1);
		if(world.getControllerSystem().controls()) dragged_id = world.getControllerSystem().dragged_world_body_id;
		DynamicArray<BodyCollider> colliders;

		for(WorldBodyId id : world.getWorldBodySystem().getUsedWorldBodyIds())
		{
			const WorldBody& body = world.getWorldBodySystem().getWorldBody(id);
			if(body.planet_id != static_cast<PlanetId>(-1) || !hasSpatialPoints(body)) continue;
			BodyCollider collider = createCollider(world, id, dragged_id);
			if(collider.parts.size() > 0) colliders.push(static_cast<BodyCollider&&>(collider));
		}
		for(uint8 iteration = 0; iteration < 4; iteration++)
		{
			for(size_t i = 0; i < colliders.size(); i++)
			{
				const BodyCollider& a = colliders[i];
				WorldBody& body_a = world.getWorldBodySystem().getWorldBody(a.id);
				for(size_t j = i + 1; j < colliders.size(); j++)
				{
					const BodyCollider& b = colliders[j];
					if(a.inverse_mass + b.inverse_mass <= 0.0) continue;
					WorldBody& body_b = world.getWorldBodySystem().getWorldBody(b.id);
					const glm::dvec3 delta = body_b.position - body_a.position;
					const double radius = a.radius + b.radius;
					if(glm::dot(delta, delta) > radius * radius) continue;
					for(const BuildPartCollider& part_a : a.parts)
					{
						for(const BuildPartCollider& part_b : b.parts)
						{
							BodyContact contact;
							if(findContact(part_a, part_b, body_b.position - body_a.position, contact))
								resolvePair(body_a, body_b, a, b, contact);
						}
					}
				}
			}
		}
	}
}
