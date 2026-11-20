#include <danikk_framework/glm.h>
#include <danikk_engine/danikk_engine.h>

#include <danikk_space_engine/assets/collection.h>
#include <danikk_space_engine/component/point/build_part.h>
#include <danikk_space_engine/component/world_body/world_body.h>
#include <danikk_space_engine/config.h>
#include <danikk_space_engine/effects/chunk_boundaries.h>
#include <danikk_space_engine/effects/unit_activity_labels.h>
#include <danikk_space_engine/effects/unit_order_indicators.h>
#include <danikk_space_engine/effects/unit_selection.h>
#include <danikk_space_engine/profiler.h>
#include <danikk_space_engine/scenario/base.h>
#include <danikk_space_engine/systems/component/component.h>
#include <danikk_space_engine/systems/controller/controller.h>
#include <danikk_space_engine/systems/unit/unit.h>
#include <danikk_space_engine/world.h>

namespace danikk_space_engine
{
	World::World()
		: chunks(*this), chunk_mesh_generation(*this), environment(*this), physics(*this), controller(*this), camera(*this),
		  components(*this), planets(*this), raycasts(*this), factions(), unit_system(*this)
	{
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

	/**
	 * @brief Ticks body and chunk components, drains deferred point deletion, then advances physics.
	 * Body, chunk, and per-body component IDs are snapshotted before callbacks can mutate ownership lists.
	 * Deferred cleanup completes before UnitSystem updates faction intelligence and orders. PhysicsSystem
	 * then advances motion and resolves contacts.
	 */
	void World::tick()
	{
		/**
		 * @brief Measures the duration of the enclosing profiler scope.
		 */
		DSE_WATCH();
		components.tickWorldBodyComponents();
		camera.tick();
		controller.tick();
		components.flushDeferredPointDestruction();
		unit_system.tick(glm::max(getTargetFrameDelay(), 0.0f));
		physics.tick();
	}

	/**
	 * @brief Accepts generated work, updates chunk/component render state, and draws the world frame.
	 *
		 * Completed chunks are accepted only when world, body, planet generation,
		 * and terrain revision still match. Dirty meshes are rebuilt on
	 * the main thread; rendering uses camera-relative transforms and optional
	 * debug effects are controlled by Settings.
	 */
	void World::frame()
	{
		/**
		 * @brief Measures the duration of the enclosing profiler scope.
		 */
		DSE_WATCH();
		setClearColor(camera.isActive()
			? planets.getAtmosphereBackgroundColor(camera.getWorldPosition()) : glm::vec3(0.0f));
		components.renderWorldBodyComponents();
		planets.update();
		chunk_mesh_generation.frame();
		static_asset_collection.base3d_shader.use();
		camera.frame();
		const vec3 camera_front = camera.getFront();
		const mat4 view = glm::lookAt(vec3(0.0f), camera_front, camera.getUp());
		setViewMatrix(view);
		chunks.renderWorld();
		// Draw dynamic chunk components after the world meshes so transparent overlays
		// do not allow later terrain draws to pass through the base mesh depth.
		components.renderChunkComponents();
		if(settings.show_chunk_bounds) drawChunkBoundaries(*this);
		drawUnitSelectionIndicators(*this);
		drawUnitOrderIndicators(*this);
		controller.renderPreview();
		drawUnitActivityLabels(*this);
		setDepthRange(0.0f, 1.0f);
	}
}
