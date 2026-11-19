#include <danikk_space_engine/profiler.h>
#include <danikk_space_engine/physics.h>
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

		/**
		 * @brief Pairs a planetary core with the radius used to remove entities that cross it.
		 */
		struct PlanetCoreTarget
		{
			WorldBodyId body_id;
			glm::dvec3 center;
		};

		/**
		 * @brief Tests a position against the fixed five-metre exclusion sphere of each planet.
		 * @param position World-space position to test.
		 * @param planets Snapshot of planet IDs and world-space centers.
		 * @return True when squared distance to any center is strictly less than 25 m².
		 */
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

		/**
		 * @brief Checks whether any active component slot marks the body as a planet.
		 * @param world World used to validate component slots.
		 * @param body Body whose component slots are inspected.
		 * @return True when an active Planet component is attached.
		 */
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

	/**
	 * @brief Destroys loaded non-planet bodies and non-camera points inside any planet core.
	 * @param world World whose WorldData resource pools are modified.
	 * Planet centers are copied before destruction. Body and point IDs are queued
	 * before release so pool iteration is not invalidated. Point components in
	 * inactive chunks are ignored; each point ID is queued at most once.
	 */
	void WorldPhysics::destroyEntitiesInsidePlanetCore()
	{
		/**
		 * @brief Measures the duration of the enclosing profiler scope.
		 */
		DSE_WATCH();
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
			world.destroyWorldBody(body_id);
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
				|| !world.chunk_system.isChunkActive(slot.chunk_id)) continue;
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
			world.destroyPoint(point_id);
		}
	}
}
