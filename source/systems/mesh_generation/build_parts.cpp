#include <danikk_space_engine/systems/mesh_generation/mesh_generation.h>
#include <danikk_space_engine/systems/world_body/system.h>
#include <danikk_space_engine/systems/build_object/system.h>
#include <danikk_space_engine/systems/build_part/system.h>
#include <danikk_space_engine/material.h>
#include <danikk_space_engine/world.h>

#include <danikk_space_engine/systems/mesh_generation/internal.h>


namespace danikk_space_engine::mesh_generation_internal
{
	/** @brief Reads a BuildObject's BuildParts and combines them into one local mesh. */
	void generateBuildPartMesh(World& world, MeshGenerationJob& job)
	{
		if(!world.getBuildObjectSystem().isBuildObjectActive(job.build_object_id)) return;
		const BuildObject& object = world.getBuildObjectSystem().getBuildObject(job.build_object_id);
		if(object.world_body_id != job.world_body_id
			|| object.generation_id != job.source_generation_id) return;
		size_t vertex_count = 0;
		size_t index_count = 0;
		bool has_mesh_origin = false;
		for(BuildPartId id : object.build_part_ids)
			if(world.getBuildPartSystem().isBuildPartActive(id))
			{
				const BuildPart& part = world.getBuildPartSystem().getBuildPart(id);
				const BuildPartGeometry& geometry = getBuildPartGeometry(part.shape);
				vertex_count += geometry.positions.size();
				index_count += geometry.triangles.size() * 3;
			}
		job.point_positions.reserve(vertex_count);
		job.triangle_point_indices.reserve(index_count);
		job.triangle_material_ids.reserve(index_count / 3);

		for(BuildPartId id : object.build_part_ids)
		{
			if(!world.getBuildPartSystem().isBuildPartActive(id)) continue;
			const BuildPart& part = world.getBuildPartSystem().getBuildPart(id);
			const BuildPartGeometry& geometry = getBuildPartGeometry(part.shape);
			if(!has_mesh_origin)
			{
				job.mesh_position = part.body_local_position;
				has_mesh_origin = true;
			}
			const vec3 position = vec3(part.body_local_position - job.mesh_position);
			const uint32 first_vertex = static_cast<uint32>(job.point_positions.size());
			for(const vec3& local_position : geometry.positions)
			{
				const vec3 scaled_position = local_position * part.size;
				const vec3 rotated_position = vec3(part.rotation * vec4(scaled_position, 0.0f));
				job.point_positions.push(position + rotated_position);
			}
			for(const CubeTriangle& triangle : geometry.triangles)
			{
				for(uint8 corner = 0; corner < 3; corner++)
					job.triangle_point_indices.push(first_vertex + triangle.points[corner]);
				job.triangle_material_ids.push(default_materials::build_part);
			}
		}
	}
}
