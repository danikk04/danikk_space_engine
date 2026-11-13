#include <danikk_space_engine/scenario/base.h>

#include <danikk_space_engine/material.h>
#include <danikk_space_engine/world/world.h>

namespace danikk_space_engine
{
	// Clears the target world and creates the base test scene.
	// Sets up the camera and its controller, creates prism and bipyramid volume cells,
	// registers them with a chunk, and generates the chunk mesh.
	void startBaseScenario(World& world)
	{
		world.data.clear();

		const PointId camera_point_id = world.data.createPoint();
		Point& camera_point = world.data.getPoint(camera_point_id);
		camera_point.position = vec3(0.0f);
		Chunk& chunk = world.data.createChunk();
		chunk.position = vec3(0.0f, 0.0f, -5.0f);
		world.camera_component_slot_id = world.data.createPointComponentSlot(
			camera_point_id,
			PointComponentTypeId::camera);
		chunk.addPointComponentSlot(world.camera_component_slot_id);
		world.local_player_controller_component_slot_id = world.data.createPointComponentSlot(
			camera_point_id,
			PointComponentTypeId::local_player_controller_component);
		chunk.addPointComponentSlot(world.local_player_controller_component_slot_id);

		const PointId first_prism_point = static_cast<PointId>(world.data.getPointCount());
		const vec3 prism_positions[] =
		{
			vec3(-1.0f, -0.5f, -0.8f),
			vec3( 0.0f, -0.5f,  0.8f),
			vec3( 1.0f, -0.5f, -0.8f),
			vec3(-1.0f,  0.5f, -0.8f),
			vec3( 0.0f,  0.5f,  0.8f),
			vec3( 1.0f,  0.5f, -0.8f)
		};

		for(uint8 i = 0; i < 6; i++)
		{
			Point& point = world.data.getPoint(world.data.createPoint());
			point.position = prism_positions[i];
		}

		const VolumeCellId prism_id = world.data.createVolumeCell();
		VolumeCell& prism = world.data.getVolumeCell(prism_id);
		chunk.volume_cell_ids.push(prism_id);
		for(uint8 i = 0; i < 6; i++)
		{
			prism.points[i] = first_prism_point + i;
		}
		prism.topology = VolumeCellTopology::triangular_prism;
		prism.material = default_materials::sandstone;
		prism.mass = 100.0f;
		prism.temperature = 293.15f;
		prism.health = 100.0f;

		const PointId first_bipyramid_point = static_cast<PointId>(world.data.getPointCount());
		const vec3 bipyramid_positions[] =
		{
			vec3(2.8f,  0.0f, -0.75f),
			vec3(2.0f,  0.0f,  0.65f),
			vec3(3.6f,  0.0f,  0.65f),
			vec3(2.8f,  0.9f,  0.0f),
			vec3(2.8f, -0.9f,  0.0f)
		};

		for(uint8 i = 0; i < 5; i++)
		{
			Point& point = world.data.getPoint(world.data.createPoint());
			point.position = bipyramid_positions[i];
		}

		const VolumeCellId bipyramid_id = world.data.createVolumeCell();
		VolumeCell& bipyramid = world.data.getVolumeCell(bipyramid_id);
		chunk.volume_cell_ids.push(bipyramid_id);
		for(uint8 i = 0; i < 5; i++)
		{
			bipyramid.points[i] = first_bipyramid_point + i;
		}
		bipyramid.topology = VolumeCellTopology::triangular_bipyramid;
		bipyramid.material = default_materials::sandstone;
		bipyramid.mass = 100.0f;
		bipyramid.temperature = 293.15f;
		bipyramid.health = 100.0f;

		chunk.createMesh();
	}
}
