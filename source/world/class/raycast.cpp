#include <danikk_framework/glm.h>

#include <danikk_space_engine/world/world.h>
#include <danikk_space_engine/math/ray.h>

namespace danikk_space_engine
{
	// Casts a ray through every triangulated face in each chunk and returns the nearest hit.
	// Normalizes the direction, transforms cell points to world space, intersects each triangle,
	// and keeps the closest hit together with its FacePtr and triangle point IDs.
	RaycastHit World::raycast(const glm::dvec3& origin, const vec3& direction, float max_distance) const
	{
		RaycastHit result;
		const float direction_length = glm::length(direction);
		if(direction_length <= 0.000001f || max_distance < 0.0f)
		{
			return result;
		}

		const vec3 normalized_direction = direction / direction_length;
		const Ray ray{vec3(0.0f), normalized_direction};
		float nearest_distance = max_distance;

		for(WorldBodyId body_id : data.getUsedWorldBodyIds())
		{
			const WorldBody& body = data.getWorldBody(body_id);
			for(ChunkId chunk_id : body.chunk_ids)
			{
				const Chunk& chunk = data.getChunk(chunk_id);
				const vec3 chunk_relative_position = vec3(getChunkWorldPosition(chunk_id) - origin);
				for(size_t cell_index = 0; cell_index < chunk.volume_cell_ids.size(); cell_index++)
				{
					const VolumeCellId cell_id = chunk.volume_cell_ids[cell_index];
					if(!data.isVolumeCellActive(cell_id)) continue;
					const VolumeCell& cell = data.getVolumeCell(cell_id);
					const VolumeCellTopologyDefinition& topology = getVolumeCellTopologyDefinition(cell.topology);
					const uint8 shared_face_mask = chunk.getSharedFaceMask(cell_index);
					for(uint8 triangle_index = 0; triangle_index < topology.triangle_count; triangle_index++)
					{
						if((shared_face_mask & static_cast<uint8>(1u << topology.triangle_face_indices[triangle_index])) != 0) continue;
						const VolumeCellTriangle& triangle = topology.triangles[triangle_index];
						const PointId point_ids[3]{cell.points[triangle.points[0]], cell.points[triangle.points[1]], cell.points[triangle.points[2]]};
						if(!data.isPointActive(point_ids[0]) || !data.isPointActive(point_ids[1]) || !data.isPointActive(point_ids[2])) continue;

						const vec3 world_a = chunk_relative_position + data.getPoint(point_ids[0]).position;
						const vec3 world_b = chunk_relative_position + data.getPoint(point_ids[1]).position;
						const vec3 world_c = chunk_relative_position + data.getPoint(point_ids[2]).position;
						const RayTriangleIntersection intersection = intersectRayTriangle(ray, world_a, world_b, world_c);
						if(!intersection.hit || intersection.distance > nearest_distance) continue;

						nearest_distance = intersection.distance;
						result.hit = true;
						result.distance = intersection.distance;
						result.position = origin + glm::dvec3(normalized_direction) * static_cast<double>(intersection.distance);
						result.normal = intersection.normal;
						result.face = FacePtr{chunk_id, cell_id, topology.triangle_face_indices[triangle_index]};
						result.triangle_index = triangle_index;
						result.points[0] = point_ids[0];
						result.points[1] = point_ids[1];
						result.points[2] = point_ids[2];
					}
				}
				for(GroundTriangleId triangle_id : chunk.ground_triangle_ids)
				{
					if(!data.isGroundTriangleActive(triangle_id)) continue;
					const GroundTriangle& triangle = data.getGroundTriangle(triangle_id);
					const vec3 a = chunk_relative_position + data.getPoint(triangle.points[0]).position;
					const vec3 b = chunk_relative_position + data.getPoint(triangle.points[1]).position;
					const vec3 c = chunk_relative_position + data.getPoint(triangle.points[2]).position;
					const RayTriangleIntersection intersection = intersectRayTriangle(ray, a, b, c);
					if(!intersection.hit || intersection.distance > nearest_distance) continue;
					nearest_distance = intersection.distance;
					result.hit = true;
					result.distance = intersection.distance;
					result.position = origin + glm::dvec3(normalized_direction) * static_cast<double>(intersection.distance);
					result.normal = intersection.normal;
					result.face = FacePtr{chunk_id, 0, 0, triangle_id, true};
					result.triangle_index = 0;
					for(uint8 point_index = 0; point_index < 3; point_index++) result.points[point_index] = triangle.points[point_index];
				}
			}
		}
		return result;
	}

	RaycastHit World::raycastFromCamera(float max_distance) const
	{
		if(camera_component_slot_id >= data.getPointComponentSlotCount())
		{
			return RaycastHit{};
		}
		if(!data.isPointComponentSlotActive(camera_component_slot_id))
		{
			return RaycastHit{};
		}
		const PointComponentSlot& camera_slot = data.getPointComponentSlot(camera_component_slot_id);
		if(!data.isPointActive(camera_slot.point_id))
		{
			return RaycastHit{};
		}
		return raycast(getCameraWorldPosition(), getCameraComponent().getFront(), max_distance);
	}
}
