#include <danikk_space_engine/profiler.h>
#include <danikk_space_engine/world_physics.h>
#include <danikk_space_engine/world/world.h>
#include <danikk_space_engine/component/point/point.h>
#include <danikk_space_engine/component/world_body/planet/planet.h>
#include "internal.h"

namespace danikk_space_engine
{
	namespace
	{
		constexpr double planet_core_death_radius = 5.0;
		constexpr double planet_core_death_radius_squared =
			planet_core_death_radius * planet_core_death_radius;

		struct PlanetCoreTarget
		{
			WorldBodyId body_id;
			glm::dvec3 center;
		};

		bool isInsideAnyPlanetCore(const glm::dvec3& position,
			const DynamicArray<PlanetCoreTarget>& planets)
		{
			for(const PlanetCoreTarget& planet : planets)
			{
				const glm::dvec3 offset = position - planet.center;
				if(glm::dot(offset, offset) < planet_core_death_radius_squared) return true;
			}
			return false;
		}

		bool isPlanetBody(const World& world, const WorldBody& body)
		{
			for(WorldBodyComponentSlotId slot_id : body.component_slot_ids)
			{
				if(world.data.isWorldBodyComponentSlotActive(slot_id)
					&& world.data.getWorldBodyComponentSlot(slot_id).type_id == WorldBodyComponentTypeId::planet)
				{
					return true;
				}
			}
			return false;
		}
	}

	// Removes non-planet bodies and point-based entities that end up within five metres of a planet
	// center. IDs are collected before destruction so resource-pool iteration remains valid.
	void WorldPhysics::destroyEntitiesInsidePlanetCore(World& world)
	{
		Watcher watcher("World::planet core entity removal");
		DynamicArray<PlanetCoreTarget> planets;
		const DynamicArray<WorldBodyId> world_body_ids(world.data.getUsedWorldBodyIds());
		for(WorldBodyId body_id : world_body_ids)
		{
			if(!world.data.isWorldBodyActive(body_id)) continue;
			const WorldBody& body = world.data.getWorldBody(body_id);
			if(isPlanetBody(world, body)) planets.push(PlanetCoreTarget{body_id, body.position});
		}
		if(planets.size() == 0) return;

		DynamicArray<WorldBodyId> bodies_to_destroy;
		for(WorldBodyId body_id : world_body_ids)
		{
			if(!world.data.isWorldBodyActive(body_id)) continue;
			const WorldBody& body = world.data.getWorldBody(body_id);
			if(isPlanetBody(world, body) || !collision_internal::hasLoadedChunk(world, body)) continue;
			if(isInsideAnyPlanetCore(body.position, planets)) bodies_to_destroy.push(body_id);
		}
		for(WorldBodyId body_id : bodies_to_destroy)
		{
			world.data.destroyWorldBody(body_id);
		}

		PointId camera_point_id = static_cast<PointId>(-1);
		if(world.data.isPointComponentSlotActive(world.camera_component_slot_id))
		{
			camera_point_id = world.data.getPointComponentSlot(world.camera_component_slot_id).point_id;
		}

		DynamicArray<PointId> points_to_destroy;
		for(PointComponentSlotId slot_id : world.data.getUsedPointComponentSlotIds())
		{
			if(!world.data.isPointComponentSlotActive(slot_id)) continue;
			const PointComponentSlot& slot = world.data.getPointComponentSlot(slot_id);
			if(slot.point_id == camera_point_id || !world.data.isPointActive(slot.point_id)
				|| !world.data.isChunkActive(slot.chunk_id)) continue;
			const glm::dvec3 position = world.getPointWorldPosition(slot.chunk_id, slot.point_id);
			if(!isInsideAnyPlanetCore(position, planets)) continue;

			bool already_queued = false;
			for(PointId point_id : points_to_destroy)
			{
				if(point_id == slot.point_id)
				{
					already_queued = true;
					break;
				}
			}
			if(!already_queued) points_to_destroy.push(slot.point_id);
		}
		for(PointId point_id : points_to_destroy)
		{
			world.data.destroyPoint(point_id);
		}
	}
}
