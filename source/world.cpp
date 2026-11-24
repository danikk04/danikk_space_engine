#include <danikk_framework/glm.h>
#include <danikk_space_engine/systems/world_body/system.h>
#include <danikk_engine/danikk_engine.h>

#include <danikk_space_engine/assets/collection.h>
#include <danikk_space_engine/config.h>
#include <danikk_space_engine/effects/unit_activity_labels.h>
#include <danikk_space_engine/effects/unit_order_indicators.h>
#include <danikk_space_engine/effects/unit_selection.h>
#include <danikk_space_engine/systems/telemetry/telemetry.h>
#include <danikk_space_engine/systems/build_object/system.h>
#include <danikk_space_engine/systems/build_part/system.h>
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
		: telemetry(), point_data(world), world_body(world), build_objects(world), build_parts(world),
		  mesh_generation(world), environment(world),
		  physics(world), controller(world), camera(world), components(world), planets(world),
		  planet_render(world),
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
	WorldBodySystem& World::getWorldBodySystem() { return system_container->world_body; }
	const WorldBodySystem& World::getWorldBodySystem() const { return system_container->world_body; }
	BuildObjectSystem& World::getBuildObjectSystem() { return system_container->build_objects; }
	const BuildObjectSystem& World::getBuildObjectSystem() const { return system_container->build_objects; }
	BuildPartSystem& World::getBuildPartSystem() { return system_container->build_parts; }
	const BuildPartSystem& World::getBuildPartSystem() const { return system_container->build_parts; }
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
	PlanetRenderSystem& World::getPlanetRenderSystem() { return system_container->planet_render; }
	const PlanetRenderSystem& World::getPlanetRenderSystem() const { return system_container->planet_render; }
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

	void World::syncPlanetWorldBodyFrames()
	{
		for(const PlanetData& planet : getPlanetSystem().getPlanets())
		{
			if(!getWorldBodySystem().isWorldBodyActive(planet.world_body_id)) continue;
			WorldBody& body = getWorldBodySystem().getWorldBody(planet.world_body_id);
			body.position = planet.position;
			body.rotation = planet.rotation;
		}
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
		getBuildObjectSystem().init();
		getBuildPartSystem().init();
		while(getWorldBodySystem().getUsedWorldBodyIds().size() > 0)
		{
			const DynamicArray<WorldBodyId>& body_ids = getWorldBodySystem().getUsedWorldBodyIds();
			getWorldBodySystem().destroyWorldBody(body_ids[body_ids.size() - 1]);
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
		getPlanetRenderSystem().shutdown();
		getMeshGenerationSystem().shutdown();
		getEnvironmentSystem().shutdown();
		getPhysicsSystem().shutdown();
		getRaycastSystem().shutdown();
		getSaveSystem().shutdown();
		while(getWorldBodySystem().getUsedWorldBodyIds().size() > 0)
		{
			const DynamicArray<WorldBodyId>& body_ids = getWorldBodySystem().getUsedWorldBodyIds();
			getWorldBodySystem().destroyWorldBody(body_ids[body_ids.size() - 1]);
		}
		getPointData().release();
		getWorldBodySystem().release();
		getBuildObjectSystem().release();
		getBuildPartSystem().release();
		getBuildObjectSystem().shutdown();
		getBuildPartSystem().shutdown();
		getFactionSystem().shutdown();
		getComponentSystem().shutdown();
		getUnitSystem().shutdown();
		getHumanSystem().shutdown();
	}

	/**
	 * @brief Ticks point components, drains deferred point deletion, then advances physics.
	 * Point component IDs are snapshotted before callbacks can mutate ownership lists.
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
		getPlanetRenderSystem().update();
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
		for(WorldBodyId id : getWorldBodySystem().getUsedWorldBodyIds())
		{
			const WorldBody& body = getWorldBodySystem().getWorldBody(id);
			const BuildObjectId object_id = getBuildObjectSystem().findBuildObjectId(id);
			if(object_id == static_cast<BuildObjectId>(-1)) continue;
			const BuildObject& object = getBuildObjectSystem().getBuildObject(object_id);
			if(!object.detail_mesh_bounds_valid) continue;
			const glm::dvec3 center = body.position + body.rotation
				* (object.detail_mesh_position + glm::dvec3(object.detail_mesh_bounds_center));
			far_render_distance = glm::max(far_render_distance,
				glm::length(center - camera_position) + object.detail_mesh_bounds_radius);
		}

		const glm::dmat4 view = glm::lookAt(glm::dvec3(0.0),
			glm::dvec3(getCameraSystem().getFront()), glm::dvec3(getCameraSystem().getUp()));
		const glm::dmat4 standard_projection = glm::perspective(glm::radians(
			static_cast<double>(getCameraSystem().vertical_fov_degrees)),
			static_cast<double>(screen_ratio_gz),
			static_cast<double>(getCameraSystem().near_clip_distance),
			static_cast<double>(getCameraSystem().far_clip_distance));
		const auto draw_world_geometry = [this, &view](const glm::dmat4& projection)
		{
			static_asset_collection.base3d_shader.use();
			setViewMatrix(mat4(view));
			setProjectionMatrix(mat4(projection));
			for(WorldBodyId id : getWorldBodySystem().getUsedWorldBodyIds())
			{
				const WorldBody& body = getWorldBodySystem().getWorldBody(id);
				const BuildObjectId object_id = getBuildObjectSystem().findBuildObjectId(id);
				if(object_id == static_cast<BuildObjectId>(-1)) continue;
				const BuildObject& object = getBuildObjectSystem().getBuildObject(object_id);
				if(object.detail_mesh.mesh == nullptr) continue;
				const glm::dvec3 relative_position = body.position
					+ body.rotation * object.detail_mesh_position - getCameraSystem().getWorldPosition();
				mat4 rotation(1.0f);
				rotation[0] = vec4(vec3(body.rotation * glm::dvec3(1.0, 0.0, 0.0)), 0.0f);
				rotation[1] = vec4(vec3(body.rotation * glm::dvec3(0.0, 1.0, 0.0)), 0.0f);
				rotation[2] = vec4(vec3(body.rotation * glm::dvec3(0.0, 0.0, 1.0)), 0.0f);
				drawMeshRenderData(object.detail_mesh,
					glm::translate(mat4(1.0f), vec3(relative_position)) * rotation);
			}
			static_asset_collection.planet_terrain_shader.use();
			setViewMatrix(mat4(view));
			setProjectionMatrix(mat4(projection));
			getPlanetRenderSystem().renderPlanetTerrainMeshes();
			static_asset_collection.base3d_shader.use();
			setViewMatrix(mat4(view));
			setProjectionMatrix(mat4(projection));
		};

		if(far_render_distance > getCameraSystem().far_clip_distance * 1.05)
		{
			float near_clip = getCameraSystem().far_clip_distance * 0.95f;
			if(closest_planet_surface_distance < std::numeric_limits<double>::max())
				near_clip = glm::min(near_clip, static_cast<float>(glm::max(
					static_cast<double>(getCameraSystem().near_clip_distance), closest_planet_surface_distance * 0.5)));
			const float far_clip = static_cast<float>(glm::min(far_render_distance * 1.02,
				static_cast<double>(std::numeric_limits<float>::max() / 2.0f)));
			const glm::dmat4 far_projection = glm::perspective(glm::radians(
				static_cast<double>(getCameraSystem().vertical_fov_degrees)),
				static_cast<double>(screen_ratio_gz), static_cast<double>(near_clip),
				static_cast<double>(far_clip));
			setProjectionMatrix(mat4(far_projection));
			setDepthRange(0.5f, 1.0f);
			draw_world_geometry(far_projection);
			getCameraSystem().frame();
			setDepthRange(0.0f, 0.5f);
			draw_world_geometry(standard_projection);
		}
		else
		{
			setDepthRange(0.0f, 1.0f);
			draw_world_geometry(standard_projection);
		}
	}
}
