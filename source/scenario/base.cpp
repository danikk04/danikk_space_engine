#include <danikk_space_engine/scenario/base.h>

#include <danikk_space_engine/config.h>
#include <danikk_space_engine/systems/planet/system.h>
#include <danikk_space_engine/world.h>
#include <danikk_space_engine/systems/controller/controller.h>

#include <cmath>

namespace danikk_space_engine
{
	namespace
	{
		/**
		 * @brief Finds a deterministic camera sample just below the surface in solid terrain.
		 * @param planet_body_id Planet whose persisted terrain edits are loaded for sampling.
		 * @param planet_settings Radius, seed, and protected core settings for the scalar field.
		 * @param camera_position Receives the first qualifying world-body-local position.
		 * @return True when a sample is found; false when the planet is too small or
		 *         all tested radial directions lack the required solid depth.
		 * Scans a bounded depth below the edited heightmap in ten fixed directions.
		 * Starting from each direction's actual surface avoids scanning from a
		 * planet-wide edit radius, while the depth limit prevents a cave opening from
		 * triggering millions of expensive density samples toward the core.
		 */
		bool findGroundCameraPosition(
			const World& world,
			WorldBodyId planet_body_id,
			const PlanetSettings& planet_settings,
			glm::dvec3& camera_position)
		{
			constexpr double surface_margin = planet_voxel_size * 2.0;
			constexpr double maximum_search_depth = 512.0;
			const double core_clearance = planet_settings.core_radius + surface_margin;
			if(planet_settings.radius <= core_clearance + planet_voxel_size) return false;
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
				return world.planets.getPlanetDensity(planet_body_id, position,
					PlanetTerrainMode::cave_field_with_edits);
			};

			for(const glm::dvec3& direction : directions)
			{
				double consecutive_solid_depth = 0.0;
				const double surface_radius = world.planets.getPlanetSurfaceRadius(
					planet_body_id, direction, PlanetTerrainMode::heightmap_with_edits);
				for(double depth = step; depth <= maximum_search_depth; depth += step)
				{
					const double distance = surface_radius - depth;
					if(distance < core_clearance) break;
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

	/**
	 * @brief Clears the world data and builds the Earth planet with its camera and local controller.
	 * @param world World whose resource pools and singleton component-slot IDs are initialized.
	 * Creates the configured Earth body and PlanetSystem record, then a camera point
	 * attached directly to that body. The camera's requested spawn mode comes
	 * from Settings; a failed underground search warns and uses the sky spawn.
	 * Point callbacks are handled by ComponentSystem's global slot traversal.
	 */
	void startBaseScenario(World& world)
	{
		while(world.world_body_data.getUsedWorldBodyIds().size() > 0)
		{
			const DynamicArray<WorldBodyId>& body_ids = world.world_body_data.getUsedWorldBodyIds();
			world.destroyWorldBody(body_ids[body_ids.size() - 1]);
		}
		world.point_data.clear();
		world.world_body_data.clear();
		world.factions.create(faction_definition_ids::celestial_body);
		world.factions.create(faction_definition_ids::player);
		world.factions.create(faction_definition_ids::pirates);
		world.factions.setRelation(base_faction_ids::player, base_faction_ids::pirates,
			{FactionRelationType::hostile, 0.0f});

		const WorldBodyId planet_body_id = world.world_body_data.createWorldBody();
		world.world_body_data.getWorldBody(planet_body_id).faction_id = base_faction_ids::celestial_body;
		const PlanetSettings& planet_settings = world.planets.getPlanetSettings(planet_settings_ids::earth);
		world.planets.createPlanet(planet_body_id, planet_settings_ids::earth);
		const PointId camera_point_id = world.point_data.createPoint();
		constexpr double camera_altitude = 10.0;
		const glm::dvec3 camera_direction(0.0, 0.0, 1.0);
		glm::dvec3 camera_body_local_position = camera_direction
			* (world.planets.getPlanetSurfaceRadius(planet_body_id, camera_direction,
				PlanetTerrainMode::heightmap_with_edits) + camera_altitude);
		if(settings.default_camera_position == DefaultCameraPosition::ground
			&& !findGroundCameraPosition(world, planet_body_id, planet_settings, camera_body_local_position))
		{
			danikk_framework::formatLogWarning(
				"Unable to find solid underground camera position; spawning above the planet");
		}
		world.attachPointToBody(planet_body_id, camera_point_id, camera_body_local_position);
		world.camera.bind(camera_point_id);
		world.camera.rotation.y = -pi_half;
		world.controller.reset(camera_point_id);
	}
}
