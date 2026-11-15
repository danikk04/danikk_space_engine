#include <danikk_space_engine/scenario/base.h>

#include <danikk_space_engine/component/world_body/planet/planet.h>
#include <danikk_space_engine/component/world_body/planet/chunk_save.h>
#include <danikk_space_engine/component/point/human/human.h>
#include <danikk_space_engine/component/point/human/npc_controller.h>
#include <danikk_space_engine/world/world.h>

namespace danikk_space_engine
{
	namespace
	{
		// Finds the first solid surface beneath a point near the camera, including saved terrain edits.
		// The example has its own persistent component chunk, outside generated planet chunk records.
		void createExampleNpc(World& world, WorldBodyId planet_id, const PlanetComponentSettings& settings)
		{
			DynamicArray<PlanetTerrainEdit> edits;
			uint64 revision = 0;
			loadPlanetTerrainEdits(planet_id, settings.radius, settings.noise_seed, edits, revision);
			glm::dvec3 position(3.0, 0.0, settings.radius + planet_chunk_load_radius + planet_chunk_width * 2.0);
			const auto density = [&](const glm::dvec3& point)
			{
				return samplePlanetScalarField(point, settings.radius, settings.noise_seed, edits);
			};
			bool found_surface = false;
			while(position.z > settings.radius - planet_chunk_load_radius - planet_chunk_width * 2.0)
			{
				position.z -= 1.0;
				if(density(position) < 0.0) continue;
				double solid_z = position.z;
				double air_z = position.z + 1.0;
				for(uint8 iteration = 0; iteration < 20; iteration++)
				{
					position.z = (solid_z + air_z) * 0.5;
					if(density(position) >= 0.0) solid_z = position.z;
					else air_z = position.z;
				}
				position.z = air_z + 0.02;
				found_surface = true;
				break;
			}
			if(!found_surface) return;
			createNpc(world, planet_id, position, vec3(glm::normalize(position)), vec3(-1, 0, 0));
		}
	}

	// Clears the world, creates the planet with camera/controller, then places a wandering example NPC.
	void startBaseScenario(World& world)
	{
		world.data.clear();

		const WorldBodyId planet_body_id = world.data.createWorldBody();
		const PlanetComponentSettings planet_settings;
		world.data.createWorldBodyComponentSlot(
			planet_body_id, WorldBodyComponentTypeId::planet, &planet_settings);
		const PointId camera_point_id = world.data.createPoint();
		Point& camera_point = world.data.getPoint(camera_point_id);
		constexpr float camera_altitude = 30.0f;
		camera_point.position = vec3(0.0f);
		Chunk& chunk = world.data.createChunk(planet_body_id);
		const ChunkId camera_chunk_id = world.data.getWorldBody(planet_body_id).chunk_ids[
			world.data.getWorldBody(planet_body_id).chunk_ids.size() - 1];
		chunk.position = glm::dvec3(0.0, 0.0, default_planet_radius + camera_altitude);
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
		createExampleNpc(world, planet_body_id, planet_settings);
	}
}
