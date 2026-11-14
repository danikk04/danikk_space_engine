#include <danikk_space_engine/scenario/base.h>

#include <danikk_space_engine/world/planet.h>
#include <danikk_space_engine/world/world.h>

namespace danikk_space_engine
{
	// Clears the target world and creates the camera and controller on the planet body.
	void startBaseScenario(World& world)
	{
		world.data.clear();

		const WorldBodyId planet_body_id = world.data.createWorldBody();
		const PointId camera_point_id = world.data.createPoint();
		Point& camera_point = world.data.getPoint(camera_point_id);
		constexpr float camera_altitude = 1000.0f;
		camera_point.position = vec3(0.0f);
		Chunk& chunk = world.data.createChunk(planet_body_id);
		const ChunkId camera_chunk_id = world.data.getWorldBody(planet_body_id).chunk_ids[
			world.data.getWorldBody(planet_body_id).chunk_ids.size() - 1];
		chunk.position = glm::dvec3(0.0, 0.0, default_planet_radius + camera_altitude);
		CameraComponent camera;
		camera.rotation.x = -pi_half;
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
