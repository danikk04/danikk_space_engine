#include <danikk_space_engine/systems/mesh_generation/mesh_generation.h>
#include <danikk_space_engine/component/point/build_part.h>
#include <danikk_space_engine/material.h>
#include <danikk_space_engine/world.h>

#include <danikk_space_engine/systems/mesh_generation/internal.h>

#include <danikk_space_engine/point.h>
#include <danikk_space_engine/world_body.h>

namespace danikk_space_engine::mesh_generation_internal
{
	/** @brief Reads a body's BuildPart components and combines them into a body-local mesh. */
	void generateBuildPartMesh(World& world, MeshGenerationJob& job)
	{
		if(!world.getWorldBodyData().isWorldBodyActive(job.world_body_id)) return;
		const WorldBody& body = world.getWorldBodyData().getWorldBody(job.world_body_id);
		size_t vertex_count = 0;
		size_t index_count = 0;
		bool has_mesh_origin = false;
		for(PointId point_id : body.point_ids)
		{
			if(!world.getPointData().isPointActive(point_id)) continue;
			for(PointComponentSlotId slot_id : world.getPointData().getPoint(point_id).component_slot_ids)
			{
				if(!world.getPointData().isPointComponentSlotActive(slot_id)) continue;
				const PointComponentSlot& slot = world.getPointData().getPointComponentSlot(slot_id);
				if(slot.type_id != PointComponentTypeId::build_part) continue;
				const BuildPartComponent& part = *static_cast<const BuildPartComponent*>(slot.getData());
				const BuildPartGeometry& geometry = getBuildPartGeometry(part.shape);
				vertex_count += geometry.positions.size();
				index_count += geometry.triangles.size() * 3;
			}
		}
		job.point_positions.reserve(vertex_count);
		job.triangle_point_indices.reserve(index_count);
		job.triangle_material_ids.reserve(index_count / 3);

		for(PointId point_id : body.point_ids)
		{
			if(!world.getPointData().isPointActive(point_id)) continue;
			const Point& point = world.getPointData().getPoint(point_id);
			for(PointComponentSlotId slot_id : point.component_slot_ids)
			{
				if(!world.getPointData().isPointComponentSlotActive(slot_id)) continue;
				const PointComponentSlot& slot = world.getPointData().getPointComponentSlot(slot_id);
				if(slot.type_id != PointComponentTypeId::build_part) continue;
				const BuildPartComponent& part = *static_cast<const BuildPartComponent*>(slot.getData());
				const BuildPartGeometry& geometry = getBuildPartGeometry(part.shape);
				if(!has_mesh_origin)
				{
					job.mesh_position = point.body_local_position;
					has_mesh_origin = true;
				}
				const vec3 position = vec3(point.body_local_position - job.mesh_position);
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
		return;
	}
}
