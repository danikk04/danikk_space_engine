#pragma once

#include <danikk_space_engine/point.h>
#include <danikk_space_engine/systems/world_body/system.h>
#include <danikk_space_engine/systems/camera/camera.h>
#include <danikk_space_engine/systems/component/component.h>
#include <danikk_space_engine/systems/controller/controller.h>
#include <danikk_space_engine/systems/environment/environment.h>
#include <danikk_space_engine/systems/faction/faction.h>
#include <danikk_space_engine/systems/human/system.h>
#include <danikk_space_engine/systems/mesh_generation/mesh_generation.h>
#include <danikk_space_engine/systems/physics/physics.h>
#include <danikk_space_engine/systems/planet/system.h>
#include <danikk_space_engine/systems/raycast/raycast.h>
#include <danikk_space_engine/systems/save/system.h>
#include <danikk_space_engine/systems/telemetry/telemetry.h>
#include <danikk_space_engine/systems/unit/unit.h>

namespace danikk_space_engine
{
	class World;

	/** @brief Owns World data containers and subsystems without exposing their headers through world.h. */
	struct WorldSystemContainer
	{
		TelemetrySystem telemetry;
		PointData point_data;
		WorldBodySystem world_body;
		MeshGenerationSystem mesh_generation;
		EnvironmentSystem environment;
		PhysicsSystem physics;
		ControllerSystem controller;
		CameraSystem camera;
		ComponentSystem components;
		PlanetSystem planets;
		RaycastSystem raycasts;
		SaveSystem saves;
		FactionSystem factions;
		UnitSystem unit_system;
		HumanSystem humans;

		explicit WorldSystemContainer(World& world);
	};
}
