#include <danikk_framework/glm.h>

#include <danikk_space_engine/world.h>
#include <danikk_space_engine/asset.h>
#include <danikk_space_engine/math/ray.h>

namespace danikk_space_engine
{
	World world;

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

		for(RegionId region_id = 0; region_id < regions.size(); region_id++)
		{
			const Region& region = regions[region_id];
			const mat4 region_world_matrix = region.createWorldMatrix();
			for(VolumeCellId cell_id : region.volume_cell_ids)
			{
				if(cell_id >= data.getVolumeCellCount())
				{
					continue;
				}
				const VolumeCell& cell = data.getVolumeCell(cell_id);
				const VolumeCellTopologyDefinition& topology = getVolumeCellTopologyDefinition(cell.topology);
				for(uint8 triangle_index = 0; triangle_index < topology.triangle_count; triangle_index++)
				{
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
					const vec3 world_a = vec3(region_world_matrix * vec4(a, 1.0f));
					const vec3 world_b = vec3(region_world_matrix * vec4(data.getPoint(point_ids[1]).position, 1.0f));
					const vec3 world_c = vec3(region_world_matrix * vec4(data.getPoint(point_ids[2]).position, 1.0f));
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
					result.face = FacePtr{region_id, cell_id, topology.triangle_face_indices[triangle_index]};
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
		if(controller.target_point_id >= data.getPointCount())
		{
			return RaycastHit{};
		}
		return raycast(data.getPoint(controller.target_point_id).position, controller.getFront(), max_distance);
	}

	void World::tick()
	{
		controller.tick(data);
	}

	void World::frame()
	{
		static_asset_collection.base3d_shader.use();
		const vec3 camera_position = data.getPoint(controller.target_point_id).position;
		const vec3 camera_front = controller.getFront();
		const mat4 view = glm::lookAt(camera_position, camera_position + camera_front, controller.getUp());
		const mat4 projection = glm::perspective(90.0f, screen_ratio_gz, 0.1f, 10000.0f);
		setViewMatrix(view);
		setProjectionMatrix(projection);
		for(Region& region : regions)
		{
			region.draw();
		}
		controller.frame(*this);
	}

	void World::init()
	{
		data.clear();
		regions.clear();
		controller.selected_polygons = DynamicArray<FacePtr>();

		controller.target_point_id = data.createPoint();
		Point& camera_point = data.getPoint(controller.target_point_id);
		camera_point.position = vec3(0.0f);
		controller.rotation = vec3(0.0f);
		Region& region = regions.pushCtor();
		region.position = vec3(0.0f, 0.0f, -5.0f);

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
		region.volume_cell_ids.push(prism_id);
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
		region.volume_cell_ids.push(bipyramid_id);
		for(uint8 i = 0; i < 5; i++)
		{
			bipyramid.points[i] = first_bipyramid_point + i;
		}
		bipyramid.topology = VolumeCellTopology::triangular_bipyramid;
		bipyramid.material = default_materials::sandstone;
		bipyramid.mass = 100.0f;
		bipyramid.temperature = 293.15f;
		bipyramid.health = 100.0f;

		region.createMesh();
	}
}
