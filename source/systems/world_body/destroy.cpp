#include <danikk_space_engine/systems/telemetry/telemetry.h>
#include <danikk_space_engine/systems/world_body/system.h>
#include <danikk_space_engine/world.h>
#include <danikk_space_engine/point.h>
#include <danikk_space_engine/systems/human/system.h>
#include <danikk_space_engine/systems/planet/system.h>
#include <danikk_space_engine/systems/build_object/system.h>
#include <danikk_space_engine/systems/build_part/system.h>
#include <danikk_engine/danikk_engine.h>

namespace danikk_space_engine
{
	namespace
	{
		/** @brief Repeatedly destroys the last active ID until the owning list is empty. */
		/** @brief Clears each supplied pool while retaining its allocated capacity. */
		template<typename... pool_t>
		void clearPools(pool_t&... pools) { (pools.clear(), ...); }

		/** @brief Releases the backing storage of each supplied pool. */
		template<typename... pool_t>
		void releasePools(pool_t&... pools) { (pools.releaseMemory(), ...); }

	}

	void WorldBodySystem::attachPointToBody(WorldBodyId body_id, PointId point_id,
		const glm::dvec3& body_local_position)
	{
		if(!isWorldBodyActive(body_id) || !world.getPointData().isPointActive(point_id))
			danikk_engine::formatFatalError("Cannot attach an inactive point to a WorldBody");
		Point& point = world.getPointData().getPoint(point_id);
		WorldBody& body = getWorldBody(body_id);
		if(point.world_body_id != static_cast<WorldBodyId>(-1)
			&& point.world_body_id != body_id)
			danikk_engine::formatFatalError("Point % is already attached to another WorldBody", point_id);
		if(point.world_body_id == static_cast<WorldBodyId>(-1))
		{
			point.world_body_id = body_id;
			point.world_body_point_index = body.point_ids.size();
			body.point_ids.push(point_id);
		}
		if(point.world_body_id == body_id)
			body.spatial_tree.remove(point_id, point.body_local_position);
		point.body_local_position = body_local_position;
		body.spatial_tree.insert(point_id, body_local_position);
	}

	/** @copydoc WorldBodySystem::destroyWorldBody */
	void WorldBodySystem::destroyWorldBody(WorldBodyId id)
	{
		if(!isWorldBodyActive(id)) return;
		WorldBody& body = world_bodies[id];
		world.getBuildObjectSystem().destroyBuildObjectForWorldBody(id);
		world.getHumanSystem().destroyWorldBodyHumans(id);
		while(body.point_ids.size() > 0)
			world.getPointData().destroyPoint(body.point_ids[body.point_ids.size() - 1]);
		world.getPlanetSystem().destroyPlanetForWorldBody(id);
		if(body.point_ids.size() > 0)
			danikk_engine::formatFatalError("World body % still owns points during destruction", id);
		world_bodies.free(id);
	}

	/** @copydoc WorldBodySystem::clear */
	void WorldBodySystem::clear()
	{
		for(WorldBodyId id : world_bodies.usedIds())
			world.getPlanetSystem().destroyPlanetForWorldBody(id);
		for(WorldBodyId id : world_bodies.usedIds())
			world.getBuildObjectSystem().destroyBuildObjectForWorldBody(id);
		for(WorldBodyId id : world_bodies.usedIds())
			if(world_bodies[id].point_ids.size() > 0)
				danikk_engine::formatFatalError("World body % still owns points during WorldBodySystem clear", id);
		clearPools(world_bodies);
	}

	/** @copydoc WorldBodySystem::release */
	void WorldBodySystem::release()
	{
		clear();
		releasePools(world_bodies);
	}
}
