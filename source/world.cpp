#include <danikk_framework/glm.h>
#include <danikk_space_engine/systems/world_body/system.h>
#include <danikk_engine/danikk_engine.h>

#include <danikk_space_engine/assets/collection.h>
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
		getWorldBodySystem().destroyAllWorldBodies();
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
		getWorldBodySystem().destroyAllWorldBodies();
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
		getComponentSystem().tickPointComponents();
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
		setClearColor(getCameraSystem().isActive()
			? getPlanetSystem().getAtmosphereBackgroundColor(getCameraSystem().getWorldPosition()) : glm::vec3(0.0f));
		getPlanetRenderSystem().update();
		getMeshGenerationSystem().frame();
		getPlanetRenderSystem().renderPlanetTerrainMeshes();
		getBuildObjectSystem().renderMeshes();
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

}
