#include <danikk_space_engine/profiler.h>
#include <danikk_space_engine/physics.h>
#include <danikk_space_engine/world.h>
#include <danikk_space_engine/component/point/point.h>
#include <danikk_space_engine/planet_system.h>
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

	}

	/**
	 * @brief Destroys loaded non-planet bodies and non-camera points inside any planet core.
	 * @param world World whose point and body data pools are modified.
	 * Planet centers are copied before destruction. Body and point IDs are queued
	 * before release so pool iteration is not invalidated. Point components in
	 * inactive chunks are ignored; each point ID is queued at most once.
	 */
	void PhysicsSystem::destroyEntitiesInsidePlanetCore()
	{
		/**
		 * @brief Measures the duration of the enclosing profiler scope.
		 */
		DSE_WATCH();
		DynamicArray<PlanetCoreTarget> planets;
		const DynamicArray<WorldBodyId> world_body_ids(world.world_body_data.getUsedWorldBodyIds());
		for(WorldBodyId body_id : world_body_ids)
		{
			if(!world.world_body_data.isWorldBodyActive(body_id)) continue;
			const WorldBody& body = world.world_body_data.getWorldBody(body_id);
			if(world.planets.isPlanet(body_id)) planets.push(PlanetCoreTarget{body_id, body.position});
		}
		if(planets.size() == 0) return;

		DynamicArray<WorldBodyId> bodies_to_destroy;
		for(WorldBodyId body_id : world_body_ids)
		{
			if(!world.world_body_data.isWorldBodyActive(body_id)) continue;
			const WorldBody& body = world.world_body_data.getWorldBody(body_id);
			if(world.planets.isPlanet(body_id) || !collision_internal::hasLoadedChunk(world, body)) continue;
			if(isInsideAnyPlanetCore(body.position, planets)) bodies_to_destroy.push(body_id);
		}
		for(WorldBodyId body_id : bodies_to_destroy)
		{
			world.world_body_data.destroyWorldBody(body_id, world.chunks, world.planets);
		}

		PointId camera_point_id = static_cast<PointId>(-1);

		if(world.point_data.isPointComponentSlotActive(world.camera_component_slot_id))
		{
			camera_point_id = world.point_data.getPointComponentSlot(world.camera_component_slot_id).point_id;
		}

		DynamicArray<PointId> points_to_destroy;

		for(PointComponentSlotId slot_id : world.point_data.getUsedPointComponentSlotIds())
		{
			if(!world.point_data.isPointComponentSlotActive(slot_id)) continue;
			const PointComponentSlot& slot = world.point_data.getPointComponentSlot(slot_id);
			if(slot.point_id == camera_point_id || !world.point_data.isPointActive(slot.point_id)
				|| !world.chunks.isChunkActive(slot.chunk_id)) continue;
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
			world.chunks.destroyPoint(point_id);
		}
	}
}
