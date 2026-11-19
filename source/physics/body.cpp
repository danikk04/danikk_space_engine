#include <cmath>

#include <danikk_space_engine/profiler.h>
#include <danikk_space_engine/physics.h>
#include <danikk_space_engine/controller_system.h>
#include "internal.h"

namespace danikk_space_engine
{
	using namespace collision_internal;

	namespace
	{

		/**
		 * @brief Checks whether any active component slot marks the body as a planet.
		 * @param world World used to validate component slots.
		 * @param body Body whose component slot IDs are examined.
		 * @return True on the first active Planet slot; otherwise false.
		 */
		bool isPlanetBody(const World& world, const WorldBody& body)
		{
			for(WorldBodyComponentSlotId id : body.component_slot_ids)
			{
				if(world.world_body_data.isWorldBodyComponentSlotActive(id)
					&& world.world_body_data.getWorldBodyComponentSlot(id).type_id == WorldBodyComponentTypeId::planet)
					return true;
			}
			return false;
		}
	}

	/**
	 * @brief Builds convex mesh proxies and resolves body pairs in four sequential passes.
	 * @param world World whose loaded mesh bodies may be moved and have velocities changed.
	 * Planet bodies and bodies without active chunks are excluded. Each unordered
	 * body pair is broadphase-tested by its aggregate radius, then each chunk pair
	 * is tested by a bounding sphere and SAT. The Drag-held body has zero inverse
	 * mass and inertia, so the other body receives the pair correction and impulse.
	 */
	void WorldPhysics::resolveBodyCollisions()
	{
		/**
		 * @brief Measures the duration of the enclosing profiler scope.
		 */
		DSE_WATCH();
		WorldBodyId dragged_id = static_cast<WorldBodyId>(-1);
		if(world.controller_system.controls()) dragged_id = world.controller_system.dragged_world_body_id;
		DynamicArray<BodyCollider> colliders;

		for(WorldBodyId id : world.world_body_data.getUsedWorldBodyIds())
		{
			const WorldBody& body = world.world_body_data.getWorldBody(id);
			if(isPlanetBody(world, body) || !hasLoadedChunk(world, body)) continue;
			BodyCollider collider = createCollider(world, id, dragged_id);
			if(collider.chunks.size() > 0) colliders.push(static_cast<BodyCollider&&>(collider));
		}
		for(uint8 iteration = 0; iteration < 4; iteration++)
		{
			for(size_t i = 0; i < colliders.size(); i++)
			{
				const BodyCollider& a = colliders[i];
				WorldBody& body_a = world.world_body_data.getWorldBody(a.id);
				for(size_t j = i + 1; j < colliders.size(); j++)
				{
					const BodyCollider& b = colliders[j];
					if(a.inverse_mass + b.inverse_mass <= 0.0) continue;
					WorldBody& body_b = world.world_body_data.getWorldBody(b.id);
					const glm::dvec3 delta = body_b.position - body_a.position;
					const double radius = a.radius + b.radius;
					if(glm::dot(delta, delta) > radius * radius) continue;
					for(const ConvexChunkCollider& chunk_a : a.chunks)
					{
						for(const ConvexChunkCollider& chunk_b : b.chunks)
						{
							BodyContact contact;
							if(findContact(chunk_a, chunk_b, body_b.position - body_a.position, contact))
								resolvePair(body_a, body_b, a, b, contact);
						}
					}
				}
			}
		}
	}
}
