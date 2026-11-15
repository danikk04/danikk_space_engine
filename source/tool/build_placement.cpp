#include <danikk_space_engine/tool.h>
#include <danikk_space_engine/world/world.h>
#include <danikk_space_engine/world/cube_mesh.h>
#include <danikk_space_engine/component/point/machinegun.h>
#include <danikk_space_engine/component/world_body/planet/planet.h>

#include <cmath>

namespace danikk_space_engine
{
	namespace
	{
		void createWorldBodyCubeAtHit(World& world, const RaycastHit& hit)
		{
			constexpr float cube_size = 1.0f;
			constexpr double surface_offset = 0.03;
			constexpr double chunk_center_offset = planet_chunk_width * 0.5;
			vec3 up = hit.normal;
			if(glm::length(up) < 0.0001f) up = vec3(0.0f, 0.0f, 1.0f);
			up = glm::normalize(up);

			const WorldBodyId body_id = world.data.createWorldBody();
			WorldBody& body = world.data.getWorldBody(body_id);
			body.position = hit.position
				+ glm::dvec3(up) * (static_cast<double>(cube_size) * 0.5 + surface_offset);
			body.mass = 1.0;
			body.collision_radius = cube_size * 0.5f;
			Chunk& chunk = world.data.createChunk(body_id);
			chunk.position = glm::dvec3(-chunk_center_offset);
			const ChunkId chunk_id = chunk.chunk_id;
			DynamicArray<vec3> cube_positions;
			DynamicArray<CubeTriangle> cube_triangles;
			generateCubeGeometry(cube_size, cube_positions, cube_triangles);
			DynamicArray<PointId> point_ids;
			point_ids.reserve(cube_positions.size());
			for(const vec3& position : cube_positions)
			{
				const PointId point_id = world.data.createPoint();
				world.data.getPoint(point_id).position = position
					+ vec3(static_cast<float>(chunk_center_offset));
				point_ids.push(point_id);
			}
			for(const CubeTriangle& cube_triangle : cube_triangles)
			{
				const GroundTriangleId triangle_id = world.data.createGroundTriangle();
				GroundTriangle& triangle = world.data.getGroundTriangle(triangle_id);
				for(uint8 corner = 0; corner < 3; corner++)
				{
					triangle.points[corner] = point_ids[cube_triangle.points[corner]];
				}
				world.data.addGroundTriangleToChunk(chunk_id, triangle_id);
			}
		}

		void createMachinegunAtHit(World& world, const RaycastHit& hit)
		{
			const PointId point_id = world.data.createPoint();
			Point& point = world.data.getPoint(point_id);
			const Chunk& chunk = world.data.getChunk(hit.face.chunk_id);
			const WorldBody& body = world.data.getWorldBody(chunk.world_body_id);
			const glm::dquat inverse_body_rotation = glm::conjugate(body.rotation);
			point.position = vec3(inverse_body_rotation
				* (hit.position + glm::dvec3(hit.normal * 0.03f)
				- world.getChunkWorldPosition(hit.face.chunk_id)));

			MachinegunComponent machinegun;
			machinegun.forward = vec3(inverse_body_rotation * glm::dvec3(world.getCameraComponent().getFront()));
			machinegun.up = vec3(inverse_body_rotation * glm::dvec3(world.getCameraComponent().getUp()));
			machinegun.right = vec3(inverse_body_rotation * glm::dvec3(world.getCameraComponent().getRight()));
			const PointComponentSlotId slot_id = world.data.createPointComponentSlot(
				point_id, PointComponentTypeId::machine_gun, &machinegun);
			world.data.addPointComponentSlotToChunk(hit.face.chunk_id, slot_id);
		}

	}

	// Applies Build actions that use the raycast position. Placement creates either a camera-aligned
	// machinegun component or a centered cube mesh and collider in a dedicated WorldBody.
	// Terrain extrusion edits the planet scalar field; ordinary mesh WorldBodies are unchanged.
	ToolApplyResult applyBuildPlacementTool(World& world, const RaycastHit& hit, BuildActionId action)
	{
		if(!hit.hit) return ToolApplyResult();
		switch(action)
		{
			case BuildActionId::none:
				break;
			case BuildActionId::place_machinegun:
				createMachinegunAtHit(world, hit);
				break;
			case BuildActionId::create_world_body:
				createWorldBodyCubeAtHit(world, hit);
				break;
			case BuildActionId::extrude_terrain:
				editPlanetGround(world, hit.face, 6.0, true);
				break;
			case BuildActionId::count:
				break;
		}
		return ToolApplyResult();
	}
}
