#include <danikk_space_engine/scenario/base.h>

#include <danikk_space_engine/config.h>
#include <danikk_space_engine/component/world_body/planet/planet.h>
#include <danikk_space_engine/component/world_body/planet/chunk_save.h>
#include <danikk_space_engine/component/world_body/planet/terrain.h>
#include <danikk_space_engine/world/world.h>

#include <cmath>

namespace danikk_space_engine
{
	namespace
	{
		// Searches deterministic radial directions from outside the terrain toward the core. It chooses
		// a point only after finding a continuous solid interval, keeping the camera below the surface,
		// outside the protected core, and away from thin cave walls.
		bool findGroundCameraPosition(
			WorldBodyId planet_body_id,
			const PlanetComponentSettings& planet_settings,
			glm::dvec3& camera_position)
		{
			constexpr double surface_margin = planet_voxel_size * 2.0;
			const double core_clearance = planet_settings.core_radius + surface_margin;
			if(planet_settings.radius <= core_clearance + planet_voxel_size) return false;

			DynamicArray<PlanetTerrainEdit> terrain_edits;
			uint64 terrain_revision = 0;
			loadPlanetTerrainEdits(planet_body_id, planet_settings.radius,
				planet_settings.noise_seed, terrain_edits, terrain_revision);
			double outer_radius = planet_settings.radius + 64.0;
			for(const PlanetTerrainEdit& edit : terrain_edits)
			{
				if(!edit.adds_solid_terrain) continue;
				outer_radius = glm::max(outer_radius, glm::length(edit.center) + edit.radius + surface_margin);
			}

			const glm::dvec3 directions[] = {
				glm::dvec3(0.0, 0.0, 1.0), glm::dvec3(0.0, 0.0, -1.0),
				glm::dvec3(1.0, 0.0, 0.0), glm::dvec3(-1.0, 0.0, 0.0),
				glm::dvec3(0.0, 1.0, 0.0), glm::dvec3(0.0, -1.0, 0.0),
				glm::normalize(glm::dvec3(1.0, 1.0, 1.0)),
				glm::normalize(glm::dvec3(-1.0, 1.0, 1.0)),
				glm::normalize(glm::dvec3(1.0, -1.0, 1.0)),
				glm::normalize(glm::dvec3(1.0, 1.0, -1.0))};
			const double step = planet_voxel_size;
			const double required_solid_depth = glm::min(
				surface_margin,
				glm::max(step, (planet_settings.radius - core_clearance) * 0.25));
			const auto density_at = [&](const glm::dvec3& position)
			{
				return samplePlanetScalarField(position, planet_settings.radius,
					planet_settings.noise_seed, terrain_edits, planet_settings.core_radius);
			};

			for(const glm::dvec3& direction : directions)
			{
				double consecutive_solid_depth = 0.0;
				for(double distance = outer_radius; distance > core_clearance; distance -= step)
				{
					const glm::dvec3 sample_position = direction * distance;
					if(density_at(sample_position) <= 0.0)
					{
						consecutive_solid_depth = 0.0;
						continue;
					}
					consecutive_solid_depth += step;
					if(consecutive_solid_depth >= required_solid_depth)
					{
						camera_position = sample_position;
						return true;
					}
				}
			}
			return false;
		}
	}

	// Clears the world and creates the planet with the camera and its controller.
	void startBaseScenario(World& world)
	{
		world.data.clear();

		const WorldBodyId planet_body_id = world.data.createWorldBody();
		const PlanetComponentSettings& planet_settings = getPlanetSettings(planet_settings_ids::earth);
		world.data.createWorldBodyComponentSlot(
			planet_body_id, WorldBodyComponentTypeId::planet, &planet_settings);
		const PointId camera_point_id = world.data.createPoint();
		Point& camera_point = world.data.getPoint(camera_point_id);
		constexpr float camera_altitude = 30.0f;
		camera_point.position = vec3(0.0f);
		glm::dvec3 camera_chunk_position(0.0, 0.0, planet_settings.radius + camera_altitude);
		if(settings.default_camera_position == DefaultCameraPosition::ground
			&& !findGroundCameraPosition(planet_body_id, planet_settings, camera_chunk_position))
		{
			danikk_framework::formatLogWarning(
				"Unable to find solid underground camera position; spawning above the planet");
		}
		Chunk& chunk = world.data.createChunk(planet_body_id);
		const ChunkId camera_chunk_id = world.data.getWorldBody(planet_body_id).chunk_ids[
			world.data.getWorldBody(planet_body_id).chunk_ids.size() - 1];
		chunk.position = camera_chunk_position;
		CameraComponent camera;
		camera.rotation.y = -pi_half;
		world.camera_component_slot_id = world.data.createPointComponentSlot(
			camera_point_id,
			PointComponentTypeId::camera,
			&camera);
		world.data.addPointComponentSlotToChunk(camera_chunk_id, world.camera_component_slot_id);
		world.local_player_controller_component_slot_id = world.data.createPointComponentSlot(
			camera_point_id,
			PointComponentTypeId::local_player_controller_component);
		world.data.addPointComponentSlotToChunk(camera_chunk_id, world.local_player_controller_component_slot_id);
	}
}
