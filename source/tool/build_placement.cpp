#include <danikk_space_engine/tool.h>
#include <danikk_space_engine/world/world.h>
#include <danikk_space_engine/component/point/machinegun.h>
#include <danikk_space_engine/component/world_body/planet/planet.h>

namespace danikk_space_engine
{
	namespace
	{
		void createVolumeCellAtHit(World& world, const RaycastHit& hit)
		{
			constexpr VolumeCellTopology topology = VolumeCellTopology::hexahedron;
			constexpr float half_extent = 0.5f;
			constexpr float height = 1.0f;
			vec3 local_vertices[8]
			{
				vec3(-half_extent, -half_extent, 0.0f),
				vec3(half_extent, -half_extent, 0.0f),
				vec3(half_extent, half_extent, 0.0f),
				vec3(-half_extent, half_extent, 0.0f),
				vec3(-half_extent, -half_extent, height),
				vec3(half_extent, -half_extent, height),
				vec3(half_extent, half_extent, height),
				vec3(-half_extent, half_extent, height)
			};

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
			cell.material = default_materials::stone;
			cell.mass = 1.0f;
			cell.health = 100.0f;
			for(uint8 i = 0; i < 8; i++)
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

	// Applies Build actions that use the raycast position. Placement creates either a camera-aligned
	// machinegun component or a one-meter hexahedral VolumeCell attached to the hit chunk. Terrain
	// addition records a solid-terrain edit and lets the affected chunk rebuild the surface.
	ToolApplyResult applyBuildPlacementTool(World& world, const RaycastHit& hit, BuildActionId action)
	{
		if(!hit.hit)
		{
			return ToolApplyResult();
		}

		switch(action)
		{
			case BuildActionId::none:
			case BuildActionId::extrude_face:
				break;
			case BuildActionId::place_machinegun:
				createMachinegunAtHit(world, hit);
				break;
			case BuildActionId::place_volume_cell:
				createVolumeCellAtHit(world, hit);
				break;
			case BuildActionId::add_terrain:
				if(hit.face.is_ground_triangle) editPlanetGround(world, hit.face, 6.0, true);
				break;
			case BuildActionId::count:
				break;
		}
		return ToolApplyResult();
	}
}
