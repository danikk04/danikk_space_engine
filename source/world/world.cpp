#include <danikk_framework/glm.h>

#include <danikk_space_engine/world/world.h>
#include <danikk_space_engine/asset.h>
#include <danikk_space_engine/math/ray.h>
#include <danikk_space_engine/effects/polygon_highlight.h>
#include <danikk_space_engine/scenario/base.h>

namespace danikk_space_engine
{
	World world;

	// Casts a ray through every triangulated face in each chunk and returns the nearest hit.
	// Normalizes the direction, transforms cell points to world space, intersects each triangle,
	// and keeps the closest hit together with its FacePtr and triangle point IDs.
	RaycastHit World::raycast(const vec3& origin, const vec3& direction, float max_distance) const
	{
		RaycastHit result;
		const float direction_length = glm::length(direction);
		if(direction_length <= 0.000001f || max_distance < 0.0f)
		{
			return result;
		}

		const vec3 normalized_direction = direction / direction_length;
		const Ray ray{origin, normalized_direction};
		float nearest_distance = max_distance;

		for(ChunkId chunk_id = 0; chunk_id < data.getChunkCount(); chunk_id++)
		{
			const Chunk& chunk = data.getChunk(chunk_id);
			const mat4 chunk_world_matrix = chunk.createWorldMatrix();
			for(size_t cell_index = 0; cell_index < chunk.volume_cell_ids.size(); cell_index++)
			{
				const VolumeCellId cell_id = chunk.volume_cell_ids[cell_index];
				if(cell_id >= data.getVolumeCellCount())
				{
					continue;
				}
				const VolumeCell& cell = data.getVolumeCell(cell_id);
				const VolumeCellTopologyDefinition& topology = getVolumeCellTopologyDefinition(cell.topology);
				const uint8 shared_face_mask = chunk.getSharedFaceMask(cell_index);
				for(uint8 triangle_index = 0; triangle_index < topology.triangle_count; triangle_index++)
				{
					if((shared_face_mask & static_cast<uint8>(1u << topology.triangle_face_indices[triangle_index])) != 0)
					{
						continue;
					}
					const VolumeCellTriangle& triangle = topology.triangles[triangle_index];
					const PointId point_ids[3]
					{
						cell.points[triangle.points[0]],
						cell.points[triangle.points[1]],
						cell.points[triangle.points[2]]
					};
					if(point_ids[0] >= data.getPointCount() || point_ids[1] >= data.getPointCount() || point_ids[2] >= data.getPointCount())
					{
						continue;
					}

					const vec3 a = data.getPoint(point_ids[0]).position;
					const vec3 world_a = vec3(chunk_world_matrix * vec4(a, 1.0f));
					const vec3 world_b = vec3(chunk_world_matrix * vec4(data.getPoint(point_ids[1]).position, 1.0f));
					const vec3 world_c = vec3(chunk_world_matrix * vec4(data.getPoint(point_ids[2]).position, 1.0f));
					const RayTriangleIntersection intersection = intersectRayTriangle(ray, world_a, world_b, world_c);
					if(!intersection.hit || intersection.distance > nearest_distance)
					{
						continue;
					}

					nearest_distance = intersection.distance;
					result.hit = true;
					result.distance = intersection.distance;
					result.position = origin + normalized_direction * intersection.distance;
					result.normal = intersection.normal;
					result.face = FacePtr{chunk_id, cell_id, topology.triangle_face_indices[triangle_index]};
					result.triangle_index = triangle_index;
					result.points[0] = point_ids[0];
					result.points[1] = point_ids[1];
					result.points[2] = point_ids[2];
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
		const PointComponentSlot& camera_slot = data.getPointComponentSlot(camera_component_slot_id);
		if(camera_slot.point_id >= data.getPointCount())
		{
			return RaycastHit{};
		}
		return raycast(data.getPoint(camera_slot.point_id).position, getCameraComponent().getFront(), max_distance);
	}

	PointComponentSlot& World::getCameraSlot()
	{
		return data.getPointComponentSlot(camera_component_slot_id);
	}

	const PointComponentSlot& World::getCameraSlot() const
	{
		return data.getPointComponentSlot(camera_component_slot_id);
	}

	CameraComponent& World::getCameraComponent()
	{
		return *static_cast<CameraComponent*>(getCameraSlot().getData());
	}

	const CameraComponent& World::getCameraComponent() const
	{
		return *static_cast<const CameraComponent*>(getCameraSlot().getData());
	}

	Point& World::getCameraPoint()
	{
		return data.getPoint(getCameraSlot().point_id);
	}

	const Point& World::getCameraPoint() const
	{
		return data.getPoint(getCameraSlot().point_id);
	}

	LocalPlayerControllerComponent& World::getLocalPlayerControllerComponent()
	{
		PointComponentSlot& slot = data.getPointComponentSlot(local_player_controller_component_slot_id);
		return *static_cast<LocalPlayerControllerComponent*>(slot.getData());
	}

	const LocalPlayerControllerComponent& World::getLocalPlayerControllerComponent() const
	{
		const PointComponentSlot& slot = data.getPointComponentSlot(local_player_controller_component_slot_id);
		return *static_cast<const LocalPlayerControllerComponent*>(slot.getData());
	}

	void World::tick()
	{
		for(ChunkId chunk_id = 0; chunk_id < data.getChunkCount(); chunk_id++)
		{
			data.getChunk(chunk_id).tick(*this);
		}
	}

	void World::frame()
	{
		static_asset_collection.base3d_shader.use();
		const vec3 camera_position = getCameraPoint().position;
		const CameraComponent& camera = getCameraComponent();
		const vec3 camera_front = camera.getFront();
		const mat4 view = glm::lookAt(camera_position, camera_position + camera_front, camera.getUp());
		setViewMatrix(view);
		for(ChunkId chunk_id = 0; chunk_id < data.getChunkCount(); chunk_id++)
		{
			data.getChunk(chunk_id).frame(*this);
		}
		for(ChunkId chunk_id = 0; chunk_id < data.getChunkCount(); chunk_id++)
		{
			data.getChunk(chunk_id).draw();
		}
		drawPolygonHighlights(*this);
	}

	void World::init()
	{
		startBaseScenario(*this);
	}
}
