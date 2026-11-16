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

		/**
		 * @brief Creates a one-metre mesh body just above the hit and centers its chunk frame.
		 * @param world World receiving body, chunk, point, and triangle resources.
		 * @param hit Raycast result supplying placement position and outward normal.
		 * The body's origin is placed half a cube plus 0.03 m along the hit normal.
		 * The chunk origin is offset by half the planet chunk width so the initial
		 * cube vertices lie near the chunk center and avoid boundary straddling.
		 */
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

		/**
		 * @brief Creates a point-attached machine gun in the hit chunk, oriented to the camera.
		 * @param world World receiving the point and machine-gun component slot.
		 * @param hit Surface hit used for the local point position and owning chunk.
		 * The point is offset 0.03 m from the surface, transformed into chunk-local
		 * coordinates; the weapon basis is transformed from camera axes into the
		 * owning WorldBody's local orientation.
		 */
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

	/**
	 * @brief Dispatches placement of a machine gun or standalone cube body at a valid ray hit.
	 * @param world World receiving the created resources.
	 * @param hit Raycast result defining the target chunk, point, and normal.
	 * @param action Place action; unsupported values are ignored.
	 * @return Empty selection result; a miss leaves the world unchanged.
	 */
	ToolApplyResult applyPlaceTool(World& world, const RaycastHit& hit, ToolActionId action)
	{
		if(!hit.hit) return ToolApplyResult();
		switch(action)
		{
			case ToolActionId::machinegun:
				createMachinegunAtHit(world, hit);
				break;
			case ToolActionId::create_body:
				createWorldBodyCubeAtHit(world, hit);
				break;
			case ToolActionId::none:
			case ToolActionId::extrude:
			case ToolActionId::erase:
			case ToolActionId::drag:
			case ToolActionId::create_human:
			case ToolActionId::count:
				break;
		}
		return ToolApplyResult();
	}
}
