#include <danikk_space_engine/tool.h>
#include <danikk_space_engine/world/world.h>
#include <danikk_space_engine/component/point/machinegun.h>
#include <danikk_framework/random.h>

namespace danikk_space_engine
{
	namespace
	{
		void createVolumeCellAtHit(World& world, const RaycastHit& hit)
		{
			const VolumeCellTopology topology = static_cast<VolumeCellTopology>(
				danikk_framework::default_random.number<uint8>(
					static_cast<uint8>(VolumeCellTopology::tetrahedron),
					static_cast<uint8>(VolumeCellTopology::count)));
			const float width = 0.6f + (danikk_framework::default_random.nextFloat() + 1.0f) * 0.45f;
			const float depth = 0.6f + (danikk_framework::default_random.nextFloat() + 1.0f) * 0.45f;
			const float height = 0.6f + (danikk_framework::default_random.nextFloat() + 1.0f) * 0.45f;
			vec3 local_vertices[8]{};
			uint8 point_count = 0;
			const float x = width * 0.5f;
			const float y = depth * 0.5f;

			switch(topology)
			{
				case VolumeCellTopology::tetrahedron:
					point_count = 4;
					local_vertices[0] = vec3(-x, -y, 0.0f);
					local_vertices[1] = vec3(x, -y, 0.0f);
					local_vertices[2] = vec3(0.0f, y, 0.0f);
					local_vertices[3] = vec3(0.0f, 0.0f, height);
					break;
				case VolumeCellTopology::triangular_prism:
					point_count = 6;
					local_vertices[0] = vec3(-x, -y, 0.0f);
					local_vertices[1] = vec3(x, -y, 0.0f);
					local_vertices[2] = vec3(0.0f, y, 0.0f);
					local_vertices[3] = local_vertices[0] + vec3(0.0f, 0.0f, height);
					local_vertices[4] = local_vertices[1] + vec3(0.0f, 0.0f, height);
					local_vertices[5] = local_vertices[2] + vec3(0.0f, 0.0f, height);
					break;
				case VolumeCellTopology::triangular_bipyramid:
					point_count = 5;
					local_vertices[0] = vec3(-x, -y, height * 0.5f);
					local_vertices[1] = vec3(x, -y, height * 0.5f);
					local_vertices[2] = vec3(0.0f, y, height * 0.5f);
					local_vertices[3] = vec3(0.0f, 0.0f, height);
					local_vertices[4] = vec3(0.0f, 0.0f, 0.0f);
					break;
				case VolumeCellTopology::octahedron:
					point_count = 6;
					local_vertices[0] = vec3(-x, 0.0f, height * 0.5f);
					local_vertices[1] = vec3(0.0f, -y, height * 0.5f);
					local_vertices[2] = vec3(x, 0.0f, height * 0.5f);
					local_vertices[3] = vec3(0.0f, y, height * 0.5f);
					local_vertices[4] = vec3(0.0f, 0.0f, height);
					local_vertices[5] = vec3(0.0f, 0.0f, 0.0f);
					break;
				case VolumeCellTopology::hexahedron:
					point_count = 8;
					local_vertices[0] = vec3(-x, -y, 0.0f);
					local_vertices[1] = vec3(x, -y, 0.0f);
					local_vertices[2] = vec3(x, y, 0.0f);
					local_vertices[3] = vec3(-x, y, 0.0f);
					for(uint8 i = 0; i < 4; i++) local_vertices[i + 4] = local_vertices[i] + vec3(0.0f, 0.0f, height);
					break;
				case VolumeCellTopology::none:
				case VolumeCellTopology::count:
					return;
			}

			vec3 up = hit.normal;
			if(glm::length(up) < 0.0001f) up = vec3(0.0f, 0.0f, 1.0f);
			up = glm::normalize(up);
			const vec3 reference = glm::abs(glm::dot(up, vec3(0.0f, 0.0f, 1.0f))) > 0.9f
				? vec3(0.0f, 1.0f, 0.0f) : vec3(0.0f, 0.0f, 1.0f);
			const vec3 right = glm::normalize(glm::cross(reference, up));
			const vec3 forward = glm::normalize(glm::cross(up, right));
			const glm::dvec3 chunk_origin = world.getChunkWorldPosition(hit.face.chunk_id);
			const vec3 base_position = vec3(hit.position + glm::dvec3(up * 0.03f) - chunk_origin);

			const VolumeCellId cell_id = world.data.createVolumeCell();
			VolumeCell& cell = world.data.getVolumeCell(cell_id);
			cell.topology = topology;
			cell.material = default_materials::sandstone;
			cell.mass = 1.0f;
			cell.health = 100.0f;
			for(uint8 i = 0; i < point_count; i++)
			{
				const PointId point_id = world.data.createPoint();
				Point& point = world.data.getPoint(point_id);
				point.position = base_position
					+ right * local_vertices[i].x
					+ forward * local_vertices[i].y
					+ up * local_vertices[i].z;
				cell.points[i] = point_id;
			}
			world.data.addVolumeCellToChunk(hit.face.chunk_id, cell_id);
		}

		void createMachinegunAtHit(World& world, const RaycastHit& hit)
		{
			const PointId point_id = world.data.createPoint();
			Point& point = world.data.getPoint(point_id);
			point.position = vec3(hit.position + glm::dvec3(hit.normal * 0.03f)
				- world.getChunkWorldPosition(hit.face.chunk_id));

			MachinegunComponent machinegun;
			machinegun.forward = world.getCameraComponent().getFront();
			machinegun.up = world.getCameraComponent().getUp();
			machinegun.right = world.getCameraComponent().getRight();
			const PointComponentSlotId slot_id = world.data.createPointComponentSlot(
				point_id, PointComponentTypeId::machine_gun, &machinegun);
			world.data.addPointComponentSlotToChunk(hit.face.chunk_id, slot_id);
		}
	}

	// Creates the selected object at the current raycast point. Machineguns receive a point
	// component aligned to the camera; VolumeCells get a randomly selected supported topology,
	// random dimensions, and vertices oriented to the hit surface normal. Both resources are
	// attached to the hit chunk so its mesh or component update path owns their lifetime.
	ToolApplyResult applyCreateTool(World& world, const RaycastHit& hit, CreateObjectId object)
	{
		if(!hit.hit)
		{
			return ToolApplyResult();
		}

		switch(object)
		{
			case CreateObjectId::none:
				break;
			case CreateObjectId::machinegun:
				createMachinegunAtHit(world, hit);
				break;
			case CreateObjectId::volume_cell:
				createVolumeCellAtHit(world, hit);
				break;
			case CreateObjectId::count:
				break;
		}
		return ToolApplyResult();
	}
}
