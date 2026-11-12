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

		for(uint32 region_index = 0; region_index < regions.size(); region_index++)
		{
			const Region& region = regions[region_index];
			const mat4 region_world_matrix = region.createWorldMatrix();
			for(VolumeCellId cell_id : region.volume_cell_ids)
			{
				if(cell_id >= volume_cells.size())
				{
					continue;
				}
				const VolumeCell& cell = volume_cells[cell_id];
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
					if(point_ids[0] >= points.size() || point_ids[1] >= points.size() || point_ids[2] >= points.size())
					{
						continue;
					}

					const vec3 a = points[point_ids[0]].position;
					const vec3 world_a = vec3(region_world_matrix * vec4(a, 1.0f));
					const vec3 world_b = vec3(region_world_matrix * vec4(points[point_ids[1]].position, 1.0f));
					const vec3 world_c = vec3(region_world_matrix * vec4(points[point_ids[2]].position, 1.0f));
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
					result.region_index = region_index;
					result.cell_id = cell_id;
					result.face_index = topology.triangle_face_indices[triangle_index];
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
		if(controller.target_point_id >= points.size())
		{
			return RaycastHit{};
		}
		return raycast(points[controller.target_point_id].position, controller.getFront(), max_distance);
	}

	void World::tick()
	{
		controller.tick(points);
	}

	void World::frame()
	{
		static_asset_collection.base3d_shader.use();
		const vec3 camera_position = points[controller.target_point_id].position;
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
		points.clear();
		volume_cells.clear();
		regions.clear();
		controller.selected_polygons = DynamicArray<LocalPlayerController::SelectedPolygon>();

		Point& camera_point = points.pushCtor();
		controller.target_point_id = static_cast<PointId>(points.size() - 1);
		camera_point.position = vec3(0.0f);
		controller.rotation = vec3(0.0f);
		Region& region = regions.pushCtor();
		region.position = vec3(0.0f, 0.0f, -5.0f);

		const PointId first_prism_point = static_cast<PointId>(points.size());
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
			Point& point = points.pushCtor();
			point.position = prism_positions[i];
		}

		const VolumeCellId prism_id = static_cast<VolumeCellId>(volume_cells.size());
		VolumeCell& prism = volume_cells.pushCtor();
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

		const PointId first_bipyramid_point = static_cast<PointId>(points.size());
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
			Point& point = points.pushCtor();
			point.position = bipyramid_positions[i];
		}

		const VolumeCellId bipyramid_id = static_cast<VolumeCellId>(volume_cells.size());
		VolumeCell& bipyramid = volume_cells.pushCtor();
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
