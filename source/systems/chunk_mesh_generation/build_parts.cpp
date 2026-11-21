#include <danikk_space_engine/systems/chunk_mesh_generation/chunk_mesh_generation.h>
#include <danikk_space_engine/component/point/build_part.h>
#include <danikk_space_engine/material.h>
#include <danikk_space_engine/world.h>

#include "mesh_algorithms.h"

namespace danikk_space_engine::chunk_mesh_generation_internal
{
	/** @brief Reads a chunk's live BuildPart components and combines them into an indexed mesh. */
	GeneratedChunkMesh generateBuildPartChunkMesh(World& world, const ChunkMeshGenerationRequest& request)
	{
		GeneratedChunkMesh result;
		result.generation_id = request.generation_id;
		result.source_revision = request.source_revision;
		result.world_body_id = request.world_body_id;
		result.chunk_id = request.chunk_id;
		result.source = ChunkMeshSource::build_parts;
		result.chunk_position = request.chunk_position;

		if(!world.chunks.isChunkActive(request.chunk_id)) return result;
		size_t vertex_count = 0;
		size_t index_count = 0;
		for(PointComponentSlotId slot_id : world.chunks.getBuildPartSlotIds(request.chunk_id))
		{
			if(!world.point_data.isPointComponentSlotActive(slot_id)) continue;
			const PointComponentSlot& slot = world.point_data.getPointComponentSlot(slot_id);
			if(slot.type_id != PointComponentTypeId::build_part
				|| !world.point_data.isPointActive(slot.point_id)) continue;
			const BuildPartComponent& part = *static_cast<const BuildPartComponent*>(slot.getData());
			const BuildPartGeometry& geometry = getBuildPartGeometry(part.shape);
			vertex_count += geometry.positions.size();
			index_count += geometry.triangles.size() * 3;
		}
		result.point_positions.reserve(vertex_count);
		result.triangle_point_indices.reserve(index_count);
		result.triangle_material_ids.reserve(index_count / 3);

		for(PointComponentSlotId slot_id : world.chunks.getBuildPartSlotIds(request.chunk_id))
		{
			if(!world.point_data.isPointComponentSlotActive(slot_id)) continue;
			const PointComponentSlot& slot = world.point_data.getPointComponentSlot(slot_id);
			if(slot.type_id != PointComponentTypeId::build_part
				|| !world.point_data.isPointActive(slot.point_id)) continue;
			const BuildPartComponent& part = *static_cast<const BuildPartComponent*>(slot.getData());
			const BuildPartGeometry& geometry = getBuildPartGeometry(part.shape);
			const vec3 position = world.point_data.getPoint(slot.point_id).position;
			const uint32 first_vertex = static_cast<uint32>(result.point_positions.size());
			for(const vec3& local_position : geometry.positions)
			{
				const vec3 scaled_position = local_position * part.size;
				const vec3 rotated_position = vec3(part.rotation * vec4(scaled_position, 0.0f));
				result.point_positions.push(position + rotated_position);
			}
			for(const CubeTriangle& triangle : geometry.triangles)
			{
				for(uint8 corner = 0; corner < 3; corner++)
					result.triangle_point_indices.push(first_vertex + triangle.points[corner]);
				result.triangle_material_ids.push(default_materials::build_part);
			}
		}
		return result;
	}
}
