#include <danikk_framework/glm.h>

#include <danikk_space_engine/world/world.h>

namespace danikk_space_engine
{
	struct ExtrudedPoint
	{
		ChunkId chunk_id;
		PointId source_point_id;
		PointId extruded_point_id;
		vec3 offset;
	};

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
			if(!data.isChunkActive(face.chunk_id) || !data.isVolumeCellActive(face.cell_id))
			{
				continue;
			}

			Chunk& chunk = data.getChunk(face.chunk_id);
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
			if(!data.isPointActive(normal_point_ids[0])
				|| !data.isPointActive(normal_point_ids[1])
				|| !data.isPointActive(normal_point_ids[2]))
			{
				continue;
			}

			vec3 base_positions[4];
			bool valid_points = true;
			for(uint8 point_index = 0; point_index < point_count; point_index++)
			{
				if(!data.isPointActive(base_point_ids[point_index]))
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
			data.addVolumeCellToChunk(face.chunk_id, extruded_cell_id);
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
			data.getChunk(chunk_id).createMesh();
		}
		return result;
	}

	// Deletes each selected cell through WorldData, which removes its chunk references and
	// returns the slot to the free list. Rebuilds affected meshes and clears stale selection.
	void World::deleteVolumeCells(const DynamicArray<FacePtr>& faces)
	{
		DynamicArray<VolumeCellId> deleted_cell_ids;
		DynamicArray<ChunkId> modified_chunk_ids;

		for(const FacePtr& face : faces)
		{
			if(!data.isChunkActive(face.chunk_id) || !data.isVolumeCellActive(face.cell_id))
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

			data.destroyVolumeCell(face.cell_id);
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
			data.getChunk(chunk_id).createMesh();
		}
		getLocalPlayerControllerComponent().selected_faces = DynamicArray<FacePtr>();
		getLocalPlayerControllerComponent().selected_face_count = 0;
	}
}
