#include <cmath>

#include <danikk_space_engine/profiler.h>
#include <danikk_space_engine/world_physics.h>
#include "internal.h"

namespace danikk_space_engine
{
	using namespace collision_internal;

	namespace
	{

		/**
		 * @brief Reports whether planet body.
		 */
		bool isPlanetBody(const World& world, const WorldBody& body)
		{
			for(WorldBodyComponentSlotId id : body.component_slot_ids)
			{
				if(world.data.isWorldBodyComponentSlotActive(id)
					&& world.data.getWorldBodyComponentSlot(id).type_id == WorldBodyComponentTypeId::planet)
					return true;
			}
			return false;
		}
	}

	// Builds mesh colliders and solves body/chunk pairs in four sweeps; dragged bodies are kinematic.

	/**
	 * @brief Resolves body collisions.
	 */
	void WorldPhysics::resolveBodyCollisions(World& world)
	{
		/**
		 * @brief Disables copying to ensure a measured scope is recorded once.
		 */
		Watcher watcher("World::body collisions");
		WorldBodyId dragged_id = static_cast<WorldBodyId>(-1);
		if(world.data.isPointComponentSlotActive(world.local_player_controller_component_slot_id))
			dragged_id = world.getLocalPlayerControllerComponent().dragged_world_body_id;
		DynamicArray<BodyCollider> colliders;

		for(WorldBodyId id : world.data.getUsedWorldBodyIds())
		{
			const WorldBody& body = world.data.getWorldBody(id);
			if(isPlanetBody(world, body) || !hasLoadedChunk(world, body)) continue;
			BodyCollider collider = createCollider(world, id, dragged_id);
			if(collider.chunks.size() > 0) colliders.push(static_cast<BodyCollider&&>(collider));
		}
		for(uint8 iteration = 0; iteration < 4; iteration++)
		{
			for(size_t i = 0; i < colliders.size(); i++)
			{
				const BodyCollider& a = colliders[i];
				WorldBody& body_a = world.data.getWorldBody(a.id);
				for(size_t j = i + 1; j < colliders.size(); j++)
				{
					const BodyCollider& b = colliders[j];
					if(a.inverse_mass + b.inverse_mass <= 0.0) continue;
					WorldBody& body_b = world.data.getWorldBody(b.id);
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
