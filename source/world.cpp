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
#include <danikk_space_engine/world_system_container.h>

#include <limits>

namespace danikk_space_engine
{
	WorldSystemContainer::WorldSystemContainer(World& world)
		: telemetry(), point_data(), world_body_data(), mesh_generation(world), environment(world),
		  physics(world), controller(world), camera(world), components(world), planets(world),
		  raycasts(world), saves(world), factions(), unit_system(world), humans(world)
	{
	}

	World::World()
		: system_container(std::make_unique<WorldSystemContainer>(*this))
	{
	}

	World::~World() = default;

	TelemetrySystem& World::getTelemetrySystem() { return system_container->telemetry; }
	const TelemetrySystem& World::getTelemetrySystem() const { return system_container->telemetry; }
	PointData& World::getPointData() { return system_container->point_data; }
	const PointData& World::getPointData() const { return system_container->point_data; }
	WorldBodyData& World::getWorldBodyData() { return system_container->world_body_data; }
	const WorldBodyData& World::getWorldBodyData() const { return system_container->world_body_data; }
	MeshGenerationSystem& World::getMeshGenerationSystem() { return system_container->mesh_generation; }
	const MeshGenerationSystem& World::getMeshGenerationSystem() const { return system_container->mesh_generation; }
	EnvironmentSystem& World::getEnvironmentSystem() { return system_container->environment; }
	const EnvironmentSystem& World::getEnvironmentSystem() const { return system_container->environment; }
	PhysicsSystem& World::getPhysicsSystem() { return system_container->physics; }
	const PhysicsSystem& World::getPhysicsSystem() const { return system_container->physics; }
	ControllerSystem& World::getControllerSystem() { return system_container->controller; }
	const ControllerSystem& World::getControllerSystem() const { return system_container->controller; }
	CameraSystem& World::getCameraSystem() { return system_container->camera; }
	const CameraSystem& World::getCameraSystem() const { return system_container->camera; }
	ComponentSystem& World::getComponentSystem() { return system_container->components; }
	const ComponentSystem& World::getComponentSystem() const { return system_container->components; }
	PlanetSystem& World::getPlanetSystem() { return system_container->planets; }
	const PlanetSystem& World::getPlanetSystem() const { return system_container->planets; }
	RaycastSystem& World::getRaycastSystem() { return system_container->raycasts; }
	const RaycastSystem& World::getRaycastSystem() const { return system_container->raycasts; }
	SaveSystem& World::getSaveSystem() { return system_container->saves; }
	const SaveSystem& World::getSaveSystem() const { return system_container->saves; }
	FactionSystem& World::getFactionSystem() { return system_container->factions; }
	const FactionSystem& World::getFactionSystem() const { return system_container->factions; }
	UnitSystem& World::getUnitSystem() { return system_container->unit_system; }
	const UnitSystem& World::getUnitSystem() const { return system_container->unit_system; }
	HumanSystem& World::getHumanSystem() { return system_container->humans; }
	const HumanSystem& World::getHumanSystem() const { return system_container->humans; }

	glm::dvec3 World::getPointWorldPosition(PointId point_id) const
	{
		DSE_WATCH();
		const Point& point = getPointData().getPoint(point_id);
		const WorldBody& body = getWorldBodyData().getWorldBody(point.world_body_id);
		return body.position + body.rotation * point.body_local_position;
	}

	mat4 World::getPointWorldMatrix(PointId point_id) const
	{
		const Point& point = getPointData().getPoint(point_id);
		const WorldBody& body = getWorldBodyData().getWorldBody(point.world_body_id);
		const glm::dvec3 relative_position = body.position + body.rotation * point.body_local_position
			- getCameraSystem().getWorldPosition();
		mat4 rotation(1.0f);
		rotation[0] = vec4(vec3(body.rotation * glm::dvec3(1.0, 0.0, 0.0)), 0.0f);
		rotation[1] = vec4(vec3(body.rotation * glm::dvec3(0.0, 1.0, 0.0)), 0.0f);
		rotation[2] = vec4(vec3(body.rotation * glm::dvec3(0.0, 0.0, 1.0)), 0.0f);
		return glm::translate(mat4(1.0f), vec3(relative_position)) * rotation;
	}

	void World::attachPointComponentToBody(WorldBodyId body_id, PointId point_id,
		PointComponentSlotId slot_id, const glm::dvec3& body_local_position)
	{
		if(!getPointData().isPointComponentSlotActive(slot_id))
			danikk_engine::formatFatalError("Cannot attach an inactive point component slot");
		attachPointToBody(body_id, point_id, body_local_position);
		PointComponentSlot& slot = getPointData().getPointComponentSlot(slot_id);
		slot.world_body_id = body_id;
		if(slot.type_id == PointComponentTypeId::build_part)
			getMeshGenerationSystem().invalidateBodyDetailMesh(body_id);
	}

	void World::attachPointToBody(WorldBodyId body_id, PointId point_id,
		const glm::dvec3& body_local_position)
	{
		if(!getWorldBodyData().isWorldBodyActive(body_id) || !getPointData().isPointActive(point_id))
			danikk_engine::formatFatalError("Cannot attach an inactive point to a WorldBody");
		Point& point = getPointData().getPoint(point_id);
		WorldBody& body = getWorldBodyData().getWorldBody(body_id);
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
		if(!getPointData().isPointActive(point_id)) return;
		Point& point = getPointData().getPoint(point_id);
		if(getWorldBodyData().isWorldBodyActive(point.world_body_id))
		{
			WorldBody& body = getWorldBodyData().getWorldBody(point.world_body_id);
			body.spatial_tree.remove(point_id, point.body_local_position);
			const size_t index = point.world_body_point_index;
			if(index >= body.point_ids.size() || body.point_ids[index] != point_id)
				danikk_engine::formatFatalError("Point % has an invalid WorldBody membership index", point_id);
			for(size_t move = index + 1; move < body.point_ids.size(); ++move)
				body.point_ids[move - 1] = body.point_ids[move];
			body.point_ids.resize(body.point_ids.size() - 1);
			for(size_t move = index; move < body.point_ids.size(); ++move)
				getPointData().getPoint(body.point_ids[move]).world_body_point_index = move;
			for(PointComponentSlotId slot_id : point.component_slot_ids)
				if(getPointData().isPointComponentSlotActive(slot_id)
					&& getPointData().getPointComponentSlot(slot_id).type_id == PointComponentTypeId::build_part)
					getMeshGenerationSystem().invalidateBodyDetailMesh(point.world_body_id);
		}
		getPointData().destroyPoint(point_id);
	}

	void World::destroyWorldBody(WorldBodyId body_id)
	{
		if(!getWorldBodyData().isWorldBodyActive(body_id)) return;
		WorldBody& body = getWorldBodyData().getWorldBody(body_id);
		getHumanSystem().destroyWorldBodyHumans(body_id);
		while(body.point_ids.size() > 0) destroyPoint(body.point_ids[body.point_ids.size() - 1]);
		if(body.planet_id != static_cast<PlanetId>(-1)) getPlanetSystem().destroyPlanet(body.planet_id);
		if(body.detail_mesh.mesh != nullptr)
			getTelemetrySystem().mesh_generation_telemetry.recordMeshDeletion(MeshDeletionReason::world_body_destroyed);
		getWorldBodyData().destroyWorldBody(body_id);
	}

	void World::syncPlanetWorldBodyFrames()
	{
		for(WorldBodyId body_id : getWorldBodyData().getUsedWorldBodyIds())
		{
			WorldBody& body = getWorldBodyData().getWorldBody(body_id);
			const PlanetData* planet = getPlanetSystem().findPlanet(body.planet_id);
			if(planet == nullptr) continue;
			body.position = planet->position;
			body.rotation = planet->rotation;
		}
	}

	void World::setPointBodyLocalPosition(PointId point_id, const glm::dvec3& body_local_position)
	{
		if(!getPointData().isPointActive(point_id)) return;
		Point& point = getPointData().getPoint(point_id);
		if(!getWorldBodyData().isWorldBodyActive(point.world_body_id))
			danikk_engine::formatFatalError("Cannot move point % without an active WorldBody", point_id);
		WorldBody& body = getWorldBodyData().getWorldBody(point.world_body_id);
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
		getCameraSystem().init();
		getPlanetSystem().init();
		while(getWorldBodyData().getUsedWorldBodyIds().size() > 0)
		{
			const DynamicArray<WorldBodyId>& body_ids = getWorldBodyData().getUsedWorldBodyIds();
			destroyWorldBody(body_ids[body_ids.size() - 1]);
		}
		getMeshGenerationSystem().init();
		getComponentSystem().init();
		getUnitSystem().init();
		getHumanSystem().init();
		getFactionSystem().init();
		getEnvironmentSystem().init();
		getPhysicsSystem().init();
		getControllerSystem().init();
		getRaycastSystem().init();
		getSaveSystem().init();
		startBaseScenario(*this);
	}

	/**
	 * @brief Stops and joins the worker, invalidates outstanding results, and releases all resource pools.
	 * Advances the world token and clears request state before releasing resource pools.
	 */
	void World::shutdown()
	{
		getCameraSystem().shutdown();
		getControllerSystem().shutdown();
		getPlanetSystem().shutdown();
		getMeshGenerationSystem().shutdown();
		getEnvironmentSystem().shutdown();
		getPhysicsSystem().shutdown();
		getRaycastSystem().shutdown();
		getSaveSystem().shutdown();
		clearBuildPartMeshCache();
		while(getWorldBodyData().getUsedWorldBodyIds().size() > 0)
		{
			const DynamicArray<WorldBodyId>& body_ids = getWorldBodyData().getUsedWorldBodyIds();
			destroyWorldBody(body_ids[body_ids.size() - 1]);
		}
		getPointData().release();
		getWorldBodyData().release();
		getFactionSystem().shutdown();
		getComponentSystem().shutdown();
		getUnitSystem().shutdown();
		getHumanSystem().shutdown();
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
		syncPlanetWorldBodyFrames();
		getComponentSystem().tickWorldBodyComponents();
		getComponentSystem().tickPointComponents();
		getCameraSystem().tick();
		getControllerSystem().tick();
		getComponentSystem().flushDeferredPointDestruction();
		getHumanSystem().tick();
		getUnitSystem().tick(glm::max(getTargetFrameDelay(), 0.0f));
		getPhysicsSystem().tick();
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
		syncPlanetWorldBodyFrames();
		setClearColor(getCameraSystem().isActive()
			? getPlanetSystem().getAtmosphereBackgroundColor(getCameraSystem().getWorldPosition()) : glm::vec3(0.0f));
		getComponentSystem().renderWorldBodyComponents();
		getPlanetSystem().update();
		getMeshGenerationSystem().frame();
		static_asset_collection.base3d_shader.use();
		getCameraSystem().frame();
		const vec3 camera_front = getCameraSystem().getFront();
		const mat4 view = glm::lookAt(vec3(0.0f), camera_front, getCameraSystem().getUp());
		setViewMatrix(view);
		renderWorldMeshes();
		// Draw dynamic point components after the world meshes so transparent overlays
		// do not allow later terrain draws to pass through the base mesh depth.
		getComponentSystem().renderPointComponents();
		getHumanSystem().frame();
		drawUnitSelectionIndicators(*this);
		drawUnitOrderIndicators(*this);
		getControllerSystem().renderPreview();
		drawUnitActivityLabels(*this);
		setDepthRange(0.0f, 1.0f);
	}

	void World::renderWorldMeshes()
	{
		DSE_WATCH();
		const glm::dvec3 camera_position = getCameraSystem().getWorldPosition();
		double far_render_distance = getCameraSystem().far_clip_distance;
		double closest_planet_surface_distance = std::numeric_limits<double>::max();
		for(const PlanetData& planet : getPlanetSystem().getPlanets())
		{
			const PlanetSettings& settings = getPlanetSystem().getPlanetSettings(planet.settings_id);
			const glm::dvec3 local_camera = glm::conjugate(planet.rotation) * (camera_position - planet.position);
			const double surface_height = getPlanetSystem().getPlanetHeightAboveSurface(planet.id, local_camera);
			closest_planet_surface_distance = glm::min(closest_planet_surface_distance, std::abs(surface_height));
			far_render_distance = glm::max(far_render_distance,
				glm::length(planet.position - camera_position) + settings.radius);
		}
		for(WorldBodyId id : getWorldBodyData().getUsedWorldBodyIds())
		{
			const WorldBody& body = getWorldBodyData().getWorldBody(id);
			if(!body.detail_mesh_bounds_valid) continue;
			const glm::dvec3 center = body.position + body.rotation
				* (body.detail_mesh_position + glm::dvec3(body.detail_mesh_bounds_center));
			far_render_distance = glm::max(far_render_distance,
				glm::length(center - camera_position) + body.detail_mesh_bounds_radius);
		}

		const auto draw_world_geometry = [this]()
		{
			for(WorldBodyId id : getWorldBodyData().getUsedWorldBodyIds())
			{
				const WorldBody& body = getWorldBodyData().getWorldBody(id);
				if(body.detail_mesh.mesh == nullptr) continue;
				const glm::dvec3 relative_position = body.position
					+ body.rotation * body.detail_mesh_position - getCameraSystem().getWorldPosition();
				mat4 rotation(1.0f);
				rotation[0] = vec4(vec3(body.rotation * glm::dvec3(1.0, 0.0, 0.0)), 0.0f);
				rotation[1] = vec4(vec3(body.rotation * glm::dvec3(0.0, 1.0, 0.0)), 0.0f);
				rotation[2] = vec4(vec3(body.rotation * glm::dvec3(0.0, 0.0, 1.0)), 0.0f);
				drawMeshRenderData(body.detail_mesh,
					glm::translate(mat4(1.0f), vec3(relative_position)) * rotation);
			}
			getPlanetSystem().renderSurfaceNodes();
		};

		if(far_render_distance > getCameraSystem().far_clip_distance * 1.05)
		{
			float near_clip = getCameraSystem().far_clip_distance * 0.95f;
			if(closest_planet_surface_distance < std::numeric_limits<double>::max())
				near_clip = glm::min(near_clip, static_cast<float>(glm::max(
					static_cast<double>(getCameraSystem().near_clip_distance), closest_planet_surface_distance * 0.5)));
			const float far_clip = static_cast<float>(glm::min(far_render_distance * 1.02,
				static_cast<double>(std::numeric_limits<float>::max() / 2.0f)));
			setProjectionMatrix(glm::perspective(glm::radians(getCameraSystem().vertical_fov_degrees),
				screen_ratio_gz, near_clip, far_clip));
			setDepthRange(0.5f, 1.0f);
			draw_world_geometry();
			getCameraSystem().frame();
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
