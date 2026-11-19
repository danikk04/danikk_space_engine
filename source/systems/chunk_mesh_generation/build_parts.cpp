#include <danikk_space_engine/systems/chunk_mesh_generation/chunk_mesh_generation.h>
#include <danikk_space_engine/material.h>

#include "mesh_algorithms.h"

namespace danikk_space_engine::chunk_mesh_generation_internal
{
	/** @brief Combines immutable BuildPart snapshots into one chunk-local indexed mesh. */
	GeneratedChunkMesh generateBuildPartChunkMesh(const ChunkMeshGenerationRequest& request)
	{
		GeneratedChunkMesh result;
		result.generation_id = request.generation_id;
		result.source_revision = request.source_revision;
		result.world_body_id = request.world_body_id;
		result.chunk_id = request.chunk_id;
		result.source = ChunkMeshSource::build_parts;
		result.chunk_position = request.chunk_position;

		size_t vertex_count = 0;
		size_t index_count = 0;
		for(const BuildPartMeshInstance& part : request.build_parts)
		{
			const BuildPartGeometry& geometry = getBuildPartGeometry(part.shape);
			vertex_count += geometry.positions.size();
			index_count += geometry.triangles.size() * 3;
		}
		result.point_positions.reserve(vertex_count);
		result.triangle_point_indices.reserve(index_count);
		result.triangle_material_ids.reserve(index_count / 3);

		for(const BuildPartMeshInstance& part : request.build_parts)
		{
			const BuildPartGeometry& geometry = getBuildPartGeometry(part.shape);
			const uint32 first_vertex = static_cast<uint32>(result.point_positions.size());
			for(const vec3& local_position : geometry.positions)
			{
				const vec3 scaled_position = local_position * part.size;
				const vec3 rotated_position = vec3(part.rotation * vec4(scaled_position, 0.0f));
				result.point_positions.push(part.position + rotated_position);
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
