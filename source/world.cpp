#include <danikk_framework/glm.h>
#include <danikk_engine/danikk_engine.h>

#include <danikk_space_engine/assets/collection.h>
#include <danikk_space_engine/component/point/build_part.h>
#include <danikk_space_engine/component/world_body/world_body.h>
#include <danikk_space_engine/config.h>
#include <danikk_space_engine/effects/unit_activity_labels.h>
#include <danikk_space_engine/effects/unit_order_indicators.h>
#include <danikk_space_engine/effects/unit_selection.h>
#include <danikk_space_engine/systems/telemetry/telemetry.h>
#include <danikk_space_engine/scenario/base.h>
#include <danikk_space_engine/systems/component/component.h>
#include <danikk_space_engine/systems/controller/controller.h>
#include <danikk_space_engine/systems/unit/unit.h>
#include <danikk_space_engine/world.h>

#include <limits>

namespace danikk_space_engine
{
	World::World()
		: mesh_generation(*this), environment(*this), physics(*this), controller(*this), camera(*this),
		  components(*this), planets(*this), raycasts(*this), saves(*this), factions(), unit_system(*this), humans(*this)
	{
	}

	glm::dvec3 World::getPointWorldPosition(PointId point_id) const
	{
		DSE_WATCH();
		const Point& point = point_data.getPoint(point_id);
		const WorldBody& body = world_body_data.getWorldBody(point.world_body_id);
		return body.position + body.rotation * point.body_local_position;
	}

	mat4 World::getPointWorldMatrix(PointId point_id) const
	{
		const Point& point = point_data.getPoint(point_id);
		const WorldBody& body = world_body_data.getWorldBody(point.world_body_id);
		const glm::dvec3 relative_position = body.position + body.rotation * point.body_local_position
			- camera.getWorldPosition();
		mat4 rotation(1.0f);
		rotation[0] = vec4(vec3(body.rotation * glm::dvec3(1.0, 0.0, 0.0)), 0.0f);
		rotation[1] = vec4(vec3(body.rotation * glm::dvec3(0.0, 1.0, 0.0)), 0.0f);
		rotation[2] = vec4(vec3(body.rotation * glm::dvec3(0.0, 0.0, 1.0)), 0.0f);
		return glm::translate(mat4(1.0f), vec3(relative_position)) * rotation;
	}

	void World::attachPointComponentToBody(WorldBodyId body_id, PointId point_id,
		PointComponentSlotId slot_id, const glm::dvec3& body_local_position)
	{
		if(!point_data.isPointComponentSlotActive(slot_id))
			danikk_engine::formatFatalError("Cannot attach an inactive point component slot");
		attachPointToBody(body_id, point_id, body_local_position);
		PointComponentSlot& slot = point_data.getPointComponentSlot(slot_id);
		slot.world_body_id = body_id;
		if(slot.type_id == PointComponentTypeId::build_part)
			mesh_generation.invalidateBodyDetailMesh(body_id);
	}

	void World::attachPointToBody(WorldBodyId body_id, PointId point_id,
		const glm::dvec3& body_local_position)
	{
		if(!world_body_data.isWorldBodyActive(body_id) || !point_data.isPointActive(point_id))
			danikk_engine::formatFatalError("Cannot attach an inactive point to a WorldBody");
		Point& point = point_data.getPoint(point_id);
		WorldBody& body = world_body_data.getWorldBody(body_id);
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

	void World::destroyPoint(PointId point_id)
	{
		if(!point_data.isPointActive(point_id)) return;
		Point& point = point_data.getPoint(point_id);
		if(world_body_data.isWorldBodyActive(point.world_body_id))
		{
			WorldBody& body = world_body_data.getWorldBody(point.world_body_id);
			body.spatial_tree.remove(point_id, point.body_local_position);
			const size_t index = point.world_body_point_index;
			if(index >= body.point_ids.size() || body.point_ids[index] != point_id)
				danikk_engine::formatFatalError("Point % has an invalid WorldBody membership index", point_id);
			for(size_t move = index + 1; move < body.point_ids.size(); ++move)
				body.point_ids[move - 1] = body.point_ids[move];
			body.point_ids.resize(body.point_ids.size() - 1);
			for(size_t move = index; move < body.point_ids.size(); ++move)
				point_data.getPoint(body.point_ids[move]).world_body_point_index = move;
			for(PointComponentSlotId slot_id : point.component_slot_ids)
				if(point_data.isPointComponentSlotActive(slot_id)
					&& point_data.getPointComponentSlot(slot_id).type_id == PointComponentTypeId::build_part)
					mesh_generation.invalidateBodyDetailMesh(point.world_body_id);
		}
		point_data.destroyPoint(point_id);
	}

	void World::destroyWorldBody(WorldBodyId body_id)
	{
		if(!world_body_data.isWorldBodyActive(body_id)) return;
		WorldBody& body = world_body_data.getWorldBody(body_id);
		humans.destroyWorldBodyHumans(body_id);
		while(body.point_ids.size() > 0) destroyPoint(body.point_ids[body.point_ids.size() - 1]);
		world_body_data.destroyWorldBody(body_id, planets);
	}

	void World::setPointBodyLocalPosition(PointId point_id, const glm::dvec3& body_local_position)
	{
		if(!point_data.isPointActive(point_id)) return;
		Point& point = point_data.getPoint(point_id);
		if(!world_body_data.isWorldBodyActive(point.world_body_id))
			danikk_engine::formatFatalError("Cannot move point % without an active WorldBody", point_id);
		WorldBody& body = world_body_data.getWorldBody(point.world_body_id);
		body.spatial_tree.remove(point_id, point.body_local_position);
		point.body_local_position = body_local_position;
		body.spatial_tree.insert(point_id, body_local_position);
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
		while(world_body_data.getUsedWorldBodyIds().size() > 0)
		{
			const DynamicArray<WorldBodyId>& body_ids = world_body_data.getUsedWorldBodyIds();
			destroyWorldBody(body_ids[body_ids.size() - 1]);
		}
		mesh_generation.init();
		components.init();
		unit_system.init();
		humans.init();
		factions.init();
		environment.init();
		physics.init();
		controller.init();
		raycasts.init();
		saves.init();
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
		mesh_generation.shutdown();
		environment.shutdown();
		physics.shutdown();
		raycasts.shutdown();
		saves.shutdown();
		clearBuildPartMeshCache();
		while(world_body_data.getUsedWorldBodyIds().size() > 0)
		{
			const DynamicArray<WorldBodyId>& body_ids = world_body_data.getUsedWorldBodyIds();
			destroyWorldBody(body_ids[body_ids.size() - 1]);
		}
		point_data.release();
		world_body_data.release();
		factions.shutdown();
		components.shutdown();
		unit_system.shutdown();
		humans.shutdown();
	}

	/**
	 * @brief Ticks body and point components, drains deferred point deletion, then advances physics.
	 * Point and per-body component IDs are snapshotted before callbacks can mutate ownership lists.
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
		components.tickPointComponents();
		camera.tick();
		controller.tick();
		components.flushDeferredPointDestruction();
		humans.tick();
		unit_system.tick(glm::max(getTargetFrameDelay(), 0.0f));
		physics.tick();
	}

	/**
	 * @brief Accepts generated work, updates point/component render state, and draws the world frame.
	 *
		 * Generated meshes are accepted only when world, body, planet generation,
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
		mesh_generation.frame();
		static_asset_collection.base3d_shader.use();
		camera.frame();
		const vec3 camera_front = camera.getFront();
		const mat4 view = glm::lookAt(vec3(0.0f), camera_front, camera.getUp());
		setViewMatrix(view);
		renderWorldMeshes();
		// Draw dynamic point components after the world meshes so transparent overlays
		// do not allow later terrain draws to pass through the base mesh depth.
		components.renderPointComponents();
		humans.frame();
		drawUnitSelectionIndicators(*this);
		drawUnitOrderIndicators(*this);
		controller.renderPreview();
		drawUnitActivityLabels(*this);
		setDepthRange(0.0f, 1.0f);
	}

	void World::renderWorldMeshes()
	{
		DSE_WATCH();
		const glm::dvec3 camera_position = camera.getWorldPosition();
		double far_render_distance = camera.far_clip_distance;
		double closest_planet_surface_distance = std::numeric_limits<double>::max();
		for(const PlanetData& planet : planets.getPlanets())
		{
			if(!world_body_data.isWorldBodyActive(planet.world_body_id)) continue;
			const PlanetSettings& settings = planets.getPlanetSettings(planet.settings_id);
			const WorldBody& body = world_body_data.getWorldBody(planet.world_body_id);
			const glm::dvec3 local_camera = glm::conjugate(body.rotation) * (camera_position - body.position);
			const double surface_height = planets.getPlanetHeightAboveSurface(planet.world_body_id, local_camera);
			closest_planet_surface_distance = glm::min(closest_planet_surface_distance, std::abs(surface_height));
			far_render_distance = glm::max(far_render_distance,
				glm::length(body.position - camera_position) + settings.radius);
		}
		for(WorldBodyId id : world_body_data.getUsedWorldBodyIds())
		{
			const WorldBody& body = world_body_data.getWorldBody(id);
			if(!body.detail_mesh_bounds_valid) continue;
			const glm::dvec3 center = body.position + body.rotation
				* (body.detail_mesh_position + glm::dvec3(body.detail_mesh_bounds_center));
			far_render_distance = glm::max(far_render_distance,
				glm::length(center - camera_position) + body.detail_mesh_bounds_radius);
		}

		const auto draw_world_geometry = [this]()
		{
			for(WorldBodyId id : world_body_data.getUsedWorldBodyIds())
			{
				const WorldBody& body = world_body_data.getWorldBody(id);
				if(body.detail_mesh.mesh == nullptr) continue;
				const glm::dvec3 relative_position = body.position
					+ body.rotation * body.detail_mesh_position - camera.getWorldPosition();
				mat4 rotation(1.0f);
				rotation[0] = vec4(vec3(body.rotation * glm::dvec3(1.0, 0.0, 0.0)), 0.0f);
				rotation[1] = vec4(vec3(body.rotation * glm::dvec3(0.0, 1.0, 0.0)), 0.0f);
				rotation[2] = vec4(vec3(body.rotation * glm::dvec3(0.0, 0.0, 1.0)), 0.0f);
				drawMeshRenderData(body.detail_mesh,
					glm::translate(mat4(1.0f), vec3(relative_position)) * rotation);
			}
			planets.renderSurfaceNodes();
		};

		if(far_render_distance > camera.far_clip_distance * 1.05)
		{
			float near_clip = camera.far_clip_distance * 0.95f;
			if(closest_planet_surface_distance < std::numeric_limits<double>::max())
				near_clip = glm::min(near_clip, static_cast<float>(glm::max(
					static_cast<double>(camera.near_clip_distance), closest_planet_surface_distance * 0.5)));
			const float far_clip = static_cast<float>(glm::min(far_render_distance * 1.02,
				static_cast<double>(std::numeric_limits<float>::max() / 2.0f)));
			setProjectionMatrix(glm::perspective(glm::radians(camera.vertical_fov_degrees),
				screen_ratio_gz, near_clip, far_clip));
			setDepthRange(0.5f, 1.0f);
			draw_world_geometry();
			camera.frame();
			setDepthRange(0.0f, 0.5f);
			draw_world_geometry();
		}
		else
		{
			setDepthRange(0.0f, 1.0f);
			draw_world_geometry();
		}
	}
}
