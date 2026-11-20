#include <danikk_framework/glm.h>

#include <danikk_space_engine/profiler.h>
#include <danikk_space_engine/world.h>
#include <danikk_space_engine/component/point/build_part.h>
#include <danikk_space_engine/component/world_body/world_body.h>
#include <danikk_space_engine/scenario/base.h>
#include <danikk_space_engine/systems/controller/controller.h>

namespace danikk_space_engine
{
	World::World()
		: chunks(*this), chunk_mesh_generation(*this), environment(*this), physics(*this), controller(*this), camera(*this),
		  components(*this), planets(*this), raycasts(*this), factions(), unit_system(*this)
	{
	}

	/**
	 * @brief Converts a chunk's body-local origin to double-precision world coordinates.
	 * @param chunk_id Active chunk whose body and local offset define the origin.
	 * @return Body position plus the body's rotation applied to the chunk offset.
	 */
	glm::dvec3 World::getChunkWorldPosition(ChunkId chunk_id) const
	{
		const Chunk& chunk = chunks.getChunk(chunk_id);
		const WorldBody& body = world_body_data.getWorldBody(chunk.world_body_id);
		return body.position + body.rotation * chunk.position;
	}

	/**
	 * @brief Converts a point's local position through its chunk and WorldBody transforms.
	 * @param chunk_id Active chunk containing the point.
	 * @param point_id Active point whose position is relative to that chunk.
	 * @return Double-precision position in world coordinates.
	 */
	glm::dvec3 World::getPointWorldPosition(ChunkId chunk_id, PointId point_id) const
	{
		DSE_WATCH();
		const Chunk& chunk = chunks.getChunk(chunk_id);
		const WorldBody& body = world_body_data.getWorldBody(chunk.world_body_id);
		return body.position + body.rotation
			* (chunk.position + glm::dvec3(point_data.getPoint(point_id).position));
	}

	/**
	 * @brief Starts the shared mesh generator, invalidates older results, resets queues, and creates the base scenario.
	 * Advances the world token while skipping zero, clears the submitted and local
	 * request arrays plus deferred point removals, then starts the base scenario.
	 * That scenario clears and repopulates the point and body data pools.
	 */
	void World::init()
	{
		camera.init();
		planets.init();
		chunks.init();
		while(world_body_data.getUsedWorldBodyIds().size() > 0)
		{
			const DynamicArray<WorldBodyId>& body_ids = world_body_data.getUsedWorldBodyIds();
			world_body_data.destroyWorldBody(body_ids[body_ids.size() - 1], chunks, planets);
		}
		chunk_mesh_generation.init();
		components.init();
		unit_system.init();
		factions.init();
		environment.init();
		physics.init();
		controller.init();
		raycasts.init();
		startBaseScenario(*this);
	}

	/**
	 * @brief Stops and joins the worker, invalidates outstanding results, and releases all resource pools.
	 * Advances the world token and clears request state before releasing resource pools.
	 */
	void World::shutdown()
	{
		camera.shutdown();
		controller.shutdown();
		planets.shutdown();
		chunk_mesh_generation.shutdown();
		environment.shutdown();
		physics.shutdown();
		raycasts.shutdown();
		clearBuildPartMeshCache();
		chunks.shutdown();
		while(world_body_data.getUsedWorldBodyIds().size() > 0)
		{
			const DynamicArray<WorldBodyId>& body_ids = world_body_data.getUsedWorldBodyIds();
			world_body_data.destroyWorldBody(body_ids[body_ids.size() - 1], chunks, planets);
		}
		point_data.release();
		world_body_data.release();
		factions.shutdown();
		components.shutdown();
		unit_system.shutdown();
	}
}
