#include <danikk_framework/glm.h>

#include <danikk_space_engine/world/world.h>
#include <danikk_space_engine/asset.h>
#include <danikk_space_engine/math/ray.h>
#include <danikk_space_engine/effects/polygon_highlight.h>

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

	LocalPlayerController& World::getLocalPlayerController()
	{
		PointComponentSlot& slot = data.getPointComponentSlot(local_player_controller_slot_id);
		return *static_cast<LocalPlayerController*>(slot.getData());
	}

	const LocalPlayerController& World::getLocalPlayerController() const
	{
		const PointComponentSlot& slot = data.getPointComponentSlot(local_player_controller_slot_id);
		return *static_cast<const LocalPlayerController*>(slot.getData());
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

	// Resets world data and builds the initial camera, prism, and bipyramid scene.
	// Creates points and volume cells through WorldData, assigns cell topology and material,
	// registers both cells with one chunk, then generates the chunk mesh.
	void World::init()
	{
		data.clear();

		const PointId camera_point_id = data.createPoint();
		Point& camera_point = data.getPoint(camera_point_id);
		camera_point.position = vec3(0.0f);
		Chunk& chunk = data.createChunk();
		chunk.position = vec3(0.0f, 0.0f, -5.0f);
		camera_component_slot_id = data.createPointComponentSlot(
			camera_point_id,
			PointComponentTypeId::camera);
		chunk.addPointComponentSlot(camera_component_slot_id);
		local_player_controller_slot_id = data.createPointComponentSlot(
			camera_point_id,
			PointComponentTypeId::local_player_controller);
		chunk.addPointComponentSlot(local_player_controller_slot_id);

		const PointId first_prism_point = static_cast<PointId>(data.getPointCount());
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
			Point& point = data.getPoint(data.createPoint());
			point.position = prism_positions[i];
		}

		const VolumeCellId prism_id = data.createVolumeCell();
		VolumeCell& prism = data.getVolumeCell(prism_id);
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

		const PointId first_bipyramid_point = static_cast<PointId>(data.getPointCount());
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
			Point& point = data.getPoint(data.createPoint());
			point.position = bipyramid_positions[i];
		}

		const VolumeCellId bipyramid_id = data.createVolumeCell();
		VolumeCell& bipyramid = data.getVolumeCell(bipyramid_id);
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
