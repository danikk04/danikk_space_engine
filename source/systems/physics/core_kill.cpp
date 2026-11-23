#include <danikk_space_engine/systems/telemetry/telemetry.h>
#include <danikk_space_engine/systems/physics/physics.h>
#include <danikk_space_engine/world.h>
#include <danikk_space_engine/component/point/point.h>
#include <danikk_space_engine/systems/planet/system.h>
#include <danikk_space_engine/systems/physics/internal.h>

#include <danikk_space_engine/point.h>
#include <danikk_space_engine/world_body.h>

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
	 * @brief Destroys loaded non-planet bodies and point entities inside any planet core.
	 * @param world World whose point and body data pools are modified.
	 * Planet centers are copied before destruction. Body and point IDs are queued
	 * before release so pool iteration is not invalidated. Point components in
	 * each point ID is queued at most once.
	 */
	void PhysicsSystem::destroyEntitiesInsidePlanetCore()
	{
		/**
		 * @brief Measures the duration of the enclosing profiler scope.
		 */
		DSE_WATCH();
		DynamicArray<PlanetCoreTarget> planets;
		const DynamicArray<WorldBodyId> world_body_ids(world.getWorldBodyData().getUsedWorldBodyIds());
		for(WorldBodyId body_id : world_body_ids)
		{
			if(!world.getWorldBodyData().isWorldBodyActive(body_id)) continue;
			const WorldBody& body = world.getWorldBodyData().getWorldBody(body_id);
			if(body.planet_id != static_cast<PlanetId>(-1)) planets.push(PlanetCoreTarget{body_id, body.position});
		}
		if(planets.size() == 0) return;

		DynamicArray<WorldBodyId> bodies_to_destroy;
		for(WorldBodyId body_id : world_body_ids)
		{
			if(!world.getWorldBodyData().isWorldBodyActive(body_id)) continue;
			const WorldBody& body = world.getWorldBodyData().getWorldBody(body_id);
			if(body.planet_id != static_cast<PlanetId>(-1) || !collision_internal::hasSpatialPoints(body)) continue;
			if(isInsideAnyPlanetCore(body.position, planets)) bodies_to_destroy.push(body_id);
		}
		for(WorldBodyId body_id : bodies_to_destroy)
		{
			world.getWorldBodyData().destroyWorldBody(body_id);
		}

		DynamicArray<PointId> points_to_destroy;

		for(PointComponentSlotId slot_id : world.getPointData().getUsedPointComponentSlotIds())
		{
			if(!world.getPointData().isPointComponentSlotActive(slot_id)) continue;
			const PointComponentSlot& slot = world.getPointData().getPointComponentSlot(slot_id);
			if(!world.getPointData().isPointActive(slot.point_id)
				|| !world.getWorldBodyData().isWorldBodyActive(slot.world_body_id)) continue;
			const glm::dvec3 position = world.getWorldBodyData().getPointWorldPosition(slot.point_id);
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
			world.getPointData().destroyPoint(point_id);
		}
	}
}
