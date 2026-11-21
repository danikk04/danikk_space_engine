#include <danikk_space_engine/systems/mesh_generation/mesh_generation.h>
#include <danikk_space_engine/component/point/build_part.h>
#include <danikk_space_engine/material.h>
#include <danikk_space_engine/world.h>

#include "mesh_algorithms.h"

namespace danikk_space_engine::mesh_generation_internal
{
	/** @brief Reads a body's BuildPart components and combines them into a body-local mesh. */
	GeneratedMesh generateBuildPartMesh(World& world, const MeshGenerationRequest& request)
	{
		GeneratedMesh result;
		result.generation_id = request.generation_id;
		result.source_revision = request.source_revision;
		result.world_body_id = request.world_body_id;
		result.source = MeshGenerationSource::build_parts;
		if(!world.world_body_data.isWorldBodyActive(request.world_body_id)) return result;
		const WorldBody& body = world.world_body_data.getWorldBody(request.world_body_id);
		size_t vertex_count = 0;
		size_t index_count = 0;
		bool has_mesh_origin = false;
		for(PointId point_id : body.point_ids)
		{
			if(!world.point_data.isPointActive(point_id)) continue;
			for(PointComponentSlotId slot_id : world.point_data.getPoint(point_id).component_slot_ids)
			{
				if(!world.point_data.isPointComponentSlotActive(slot_id)) continue;
				const PointComponentSlot& slot = world.point_data.getPointComponentSlot(slot_id);
				if(slot.type_id != PointComponentTypeId::build_part) continue;
				const BuildPartComponent& part = *static_cast<const BuildPartComponent*>(slot.getData());
				const BuildPartGeometry& geometry = getBuildPartGeometry(part.shape);
				vertex_count += geometry.positions.size();
				index_count += geometry.triangles.size() * 3;
			}
		}
		result.point_positions.reserve(vertex_count);
		result.triangle_point_indices.reserve(index_count);
		result.triangle_material_ids.reserve(index_count / 3);

		for(PointId point_id : body.point_ids)
		{
			if(!world.point_data.isPointActive(point_id)) continue;
			const Point& point = world.point_data.getPoint(point_id);
			for(PointComponentSlotId slot_id : point.component_slot_ids)
			{
				if(!world.point_data.isPointComponentSlotActive(slot_id)) continue;
				const PointComponentSlot& slot = world.point_data.getPointComponentSlot(slot_id);
				if(slot.type_id != PointComponentTypeId::build_part) continue;
				const BuildPartComponent& part = *static_cast<const BuildPartComponent*>(slot.getData());
				const BuildPartGeometry& geometry = getBuildPartGeometry(part.shape);
				if(!has_mesh_origin)
				{
					result.mesh_position = point.body_local_position;
					has_mesh_origin = true;
				}
				const vec3 position = vec3(point.body_local_position - result.mesh_position);
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
		}
		return result;
	}
}
