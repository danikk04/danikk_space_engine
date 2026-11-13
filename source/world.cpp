#include <danikk_framework/glm.h>

#include <danikk_space_engine/world.h>
#include <danikk_space_engine/asset.h>
#include <danikk_space_engine/math/ray.h>

namespace danikk_space_engine
{
	World world;

	struct ExtrudedPoint
	{
		ChunkId chunk_id;
		PointId source_point_id;
		PointId extruded_point_id;
		vec3 offset;
	};

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

		for(ChunkId chunk_id = 0; chunk_id < chunks.size(); chunk_id++)
		{
			const Chunk& chunk = chunks[chunk_id];
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
		if(controller.target_point_id >= data.getPointCount())
		{
			return RaycastHit{};
		}
		return raycast(data.getPoint(controller.target_point_id).position, controller.getFront(), max_distance);
	}

	// Extrudes each selected triangular or quadrilateral face into its own volume cell.
	// Reuses generated points for adjacent coplanar faces, copies the source cell properties,
	// registers prism/hexahedron cells with their chunks, rebuilds affected meshes, and returns their top faces.
	DynamicArray<FacePtr> World::extrudeFaces(const DynamicArray<FacePtr>& faces, float distance)
	{
		DynamicArray<FacePtr> result;
		DynamicArray<ExtrudedPoint> extruded_points;
		DynamicArray<ChunkId> modified_chunks;

		for(const FacePtr& face : faces)
		{
			if(face.chunk_id >= chunks.size() || face.cell_id >= data.getVolumeCellCount())
			{
				continue;
			}

			Chunk& chunk = chunks[face.chunk_id];
			if(chunk.hasSharedFace(face.cell_id, face.face_index))
			{
				continue;
			}

			const VolumeCell source_cell = data.getVolumeCell(face.cell_id);
			const VolumeCellTopologyDefinition& source_topology = getVolumeCellTopologyDefinition(source_cell.topology);
			PointId base_point_ids[8]{};
			const uint8 point_count = getVolumeCellFacePointIds(source_cell, face.face_index, base_point_ids);
			if(point_count != 3 && point_count != 4)
			{
				continue;
			}

			const VolumeCellTriangle* face_triangle = nullptr;
			for(uint8 triangle_index = 0; triangle_index < source_topology.triangle_count; triangle_index++)
			{
				if(source_topology.triangle_face_indices[triangle_index] == face.face_index)
				{
					face_triangle = &source_topology.triangles[triangle_index];
					break;
				}
			}
			if(face_triangle == nullptr)
			{
				continue;
			}

			const PointId normal_point_ids[3]
			{
				source_cell.points[face_triangle->points[0]],
				source_cell.points[face_triangle->points[1]],
				source_cell.points[face_triangle->points[2]]
			};
			if(normal_point_ids[0] >= data.getPointCount()
				|| normal_point_ids[1] >= data.getPointCount()
				|| normal_point_ids[2] >= data.getPointCount())
			{
				continue;
			}

			vec3 base_positions[4];
			bool valid_points = true;
			for(uint8 point_index = 0; point_index < point_count; point_index++)
			{
				if(base_point_ids[point_index] >= data.getPointCount())
				{
					valid_points = false;
					break;
				}
				base_positions[point_index] = data.getPoint(base_point_ids[point_index]).position;
			}
			if(!valid_points)
			{
				continue;
			}

			const vec3 normal_a = data.getPoint(normal_point_ids[0]).position;
			const vec3 normal_b = data.getPoint(normal_point_ids[1]).position;
			const vec3 normal_c = data.getPoint(normal_point_ids[2]).position;
			const vec3 normal_vector = glm::cross(normal_b - normal_a, normal_c - normal_a);
			const float normal_length = glm::length(normal_vector);
			if(normal_length <= 0.000001f)
			{
				continue;
			}
			const vec3 offset = normal_vector / normal_length * distance;
			PointId extruded_point_ids[4]{};
			for(uint8 point_index = 0; point_index < point_count; point_index++)
			{
				bool found = false;
				for(const ExtrudedPoint& cached_point : extruded_points)
				{
					if(cached_point.chunk_id == face.chunk_id
						&& cached_point.source_point_id == base_point_ids[point_index]
						&& glm::length(cached_point.offset - offset) <= 0.0001f)
					{
						extruded_point_ids[point_index] = cached_point.extruded_point_id;
						found = true;
						break;
					}
				}
				if(!found)
				{
					const PointId extruded_point_id = data.createPoint();
					data.getPoint(extruded_point_id).position = base_positions[point_index] + offset;
					extruded_points.pushCtor(ExtrudedPoint{face.chunk_id, base_point_ids[point_index], extruded_point_id, offset});
					extruded_point_ids[point_index] = extruded_point_id;
				}
			}

			const VolumeCellId extruded_cell_id = data.createVolumeCell();
			VolumeCell& extruded_cell = data.getVolumeCell(extruded_cell_id);
			extruded_cell.topology = point_count == 3
				? VolumeCellTopology::triangular_prism
				: VolumeCellTopology::hexahedron;
			extruded_cell.material = source_cell.material;
			extruded_cell.mass = source_cell.mass;
			extruded_cell.temperature = source_cell.temperature;
			extruded_cell.health = source_cell.health;
			for(uint8 point_index = 0; point_index < point_count; point_index++)
			{
				extruded_cell.points[point_index] = base_point_ids[point_index];
				extruded_cell.points[point_count + point_index] = extruded_point_ids[point_index];
			}
			chunk.volume_cell_ids.push(extruded_cell_id);
			result.push(FacePtr{face.chunk_id, extruded_cell_id, 1});

			bool already_modified = false;
			for(ChunkId modified_chunk_id : modified_chunks)
			{
				if(modified_chunk_id == face.chunk_id)
				{
					already_modified = true;
					break;
				}
			}
			if(!already_modified)
			{
				modified_chunks.push(face.chunk_id);
			}
		}

		for(ChunkId chunk_id : modified_chunks)
		{
			chunks[chunk_id].createMesh();
		}
		return result;
	}

	// Removes selected cells from their owning chunks while preserving global IDs.
	// Deleted cells are marked with the `none` topology, affected meshes are rebuilt,
	// and the controller selection is cleared to invalidate removed face references.
	void World::deleteVolumeCells(const DynamicArray<FacePtr>& faces)
	{
		DynamicArray<VolumeCellId> deleted_cell_ids;
		DynamicArray<ChunkId> modified_chunk_ids;

		for(const FacePtr& face : faces)
		{
			if(face.chunk_id >= chunks.size() || face.cell_id >= data.getVolumeCellCount())
			{
				continue;
			}

			bool already_deleted = false;
			for(VolumeCellId deleted_cell_id : deleted_cell_ids)
			{
				if(deleted_cell_id == face.cell_id)
				{
					already_deleted = true;
					break;
				}
			}
			if(already_deleted)
			{
				continue;
			}

			Chunk& chunk = chunks[face.chunk_id];
			size_t cell_index = chunk.volume_cell_ids.size();
			for(size_t i = 0; i < chunk.volume_cell_ids.size(); i++)
			{
				if(chunk.volume_cell_ids[i] == face.cell_id)
				{
					cell_index = i;
					break;
				}
			}
			if(cell_index == chunk.volume_cell_ids.size())
			{
				continue;
			}

			for(size_t i = cell_index; i + 1 < chunk.volume_cell_ids.size(); i++)
			{
				chunk.volume_cell_ids[i] = chunk.volume_cell_ids[i + 1];
			}
			chunk.volume_cell_ids.resize(chunk.volume_cell_ids.size() - 1);
			data.getVolumeCell(face.cell_id).topology = VolumeCellTopology::none;
			deleted_cell_ids.push(face.cell_id);

			bool chunk_already_modified = false;
			for(ChunkId modified_chunk_id : modified_chunk_ids)
			{
				if(modified_chunk_id == face.chunk_id)
				{
					chunk_already_modified = true;
					break;
				}
			}
			if(!chunk_already_modified)
			{
				modified_chunk_ids.push(face.chunk_id);
			}
		}

		for(ChunkId chunk_id : modified_chunk_ids)
		{
			chunks[chunk_id].createMesh();
		}
		controller.selected_polygons = DynamicArray<FacePtr>();
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
		for(Chunk& chunk : chunks)
		{
			chunk.draw();
		}
		controller.frame(*this);
	}

	// Resets world data and builds the initial camera, prism, and bipyramid scene.
	// Creates points and volume cells through WorldData, assigns cell topology and material,
	// registers both cells with one chunk, then generates the chunk mesh.
	void World::init()
	{
		data.clear();
		chunks.clear();
		controller.selected_polygons = DynamicArray<FacePtr>();

		controller.target_point_id = data.createPoint();
		Point& camera_point = data.getPoint(controller.target_point_id);
		camera_point.position = vec3(0.0f);
		controller.rotation = vec3(0.0f);
		Chunk& chunk = chunks.pushCtor();
		chunk.position = vec3(0.0f, 0.0f, -5.0f);

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
