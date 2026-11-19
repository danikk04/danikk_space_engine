#include <cmath>

#include <danikk_space_engine/tool.h>
#include <danikk_space_engine/world/world.h>
#include <danikk_space_engine/component/point/build_part.h>
#include <danikk_space_engine/geometry/build_part.h>

namespace danikk_space_engine
{
	/** @copydoc getBuildPartPlacement */
	BuildPartPlacement getBuildPartPlacement(
		World& world, const RaycastHit& hit, BuildPartShape shape,
		const vec3& size, uint8 rotation_step)
	{
		constexpr double surface_offset = 0.03;
		constexpr double grid_step = 0.5;
		BuildPartPlacement placement;
		vec3 hit_normal = hit.normal;
		if(glm::length(hit_normal) <= 0.0001f) hit_normal = vec3(0.0f, 0.0f, 1.0f);
		else hit_normal = glm::normalize(hit_normal);

		const mat4 local_quarter_turn = glm::rotate(
			mat4(1.0f), glm::radians(90.0f * static_cast<float>(rotation_step % 4)), vec3(0.0f, 0.0f, 1.0f));
		const BuildPartGeometry& geometry = getBuildPartGeometry(shape);
		const auto getSupportExtent = [&geometry, &size](const vec3& local_normal)
		{
			float extent = 0.0f;
			for(const vec3& vertex : geometry.positions)
				extent = glm::max(extent, glm::dot(vertex * size, local_normal));
			return static_cast<double>(extent);
		};

		placement.body_local_rotation = createBuildPartRotation(hit_normal) * local_quarter_turn;
		const vec3 terrain_local_normal = vec3(
			glm::transpose(mat3(placement.body_local_rotation)) * hit_normal);
		placement.world_position = hit.position + glm::dvec3(hit_normal)
			* (getSupportExtent(terrain_local_normal) + surface_offset);
		placement.world_rotation = placement.body_local_rotation;

		if(!hit.hit_build_part || !world.data.isPointComponentSlotActive(hit.build_part_slot_id))
			return placement;

		const PointComponentSlot& parent_slot = world.data.getPointComponentSlot(hit.build_part_slot_id);
		if(parent_slot.type_id != PointComponentTypeId::build_part
			|| !world.chunk_system.isChunkActive(parent_slot.chunk_id)) return placement;

		const Chunk& chunk = world.chunk_system.getChunk(parent_slot.chunk_id);
		const WorldBody& body = world.data.getWorldBody(chunk.world_body_id);
		const BuildPartComponent& parent_part = *static_cast<const BuildPartComponent*>(parent_slot.getData());
		const glm::dquat inverse_body_rotation = glm::conjugate(body.rotation);
		const glm::dvec3 parent_center = chunk.position
			+ glm::dvec3(world.data.getPoint(parent_slot.point_id).position);
		const glm::dvec3 hit_in_body = inverse_body_rotation * (hit.position - body.position);
		const vec3 body_normal = glm::normalize(vec3(inverse_body_rotation * glm::dvec3(hit_normal)));

		// A child part inherits the parent's local frame; the hit face controls position, not its top direction.
		placement.body_local_rotation = parent_part.rotation * local_quarter_turn;
		const vec3 parent_forward(parent_part.rotation[1]);
		const vec3 parent_up(parent_part.rotation[2]);
		const vec3 tangent_reference = glm::abs(glm::dot(parent_up, body_normal)) > 0.99f
			? parent_forward : parent_up;
		const vec3 tangent_right = glm::normalize(glm::cross(tangent_reference, body_normal));
		const vec3 tangent_forward = glm::normalize(glm::cross(body_normal, tangent_right));
		const glm::dvec3 relative_hit = hit_in_body - parent_center;
		const double snapped_right = std::round(glm::dot(relative_hit, glm::dvec3(tangent_right)) / grid_step)
			* grid_step;
		const double snapped_forward = std::round(glm::dot(relative_hit, glm::dvec3(tangent_forward)) / grid_step)
			* grid_step;
		const double surface_distance = glm::dot(relative_hit, glm::dvec3(body_normal));
		const vec3 child_local_normal = vec3(
			glm::transpose(mat3(placement.body_local_rotation)) * body_normal);
		const double support_extent = getSupportExtent(child_local_normal);
		const glm::dvec3 snapped_center = parent_center
			+ glm::dvec3(tangent_right) * snapped_right
			+ glm::dvec3(tangent_forward) * snapped_forward
			+ glm::dvec3(body_normal) * (surface_distance + support_extent);
		placement.body_id = chunk.world_body_id;
		placement.chunk_id = chunk.chunk_id;
		placement.parent_slot_id = hit.build_part_slot_id;
		placement.world_position = body.position + body.rotation * snapped_center;
		placement.world_rotation = placement.body_local_rotation;
		for(uint8 axis = 0; axis < 3; ++axis)
		{
			const glm::dvec3 body_axis(placement.body_local_rotation[axis]);
			placement.world_rotation[axis] = vec4(vec3(body.rotation * body_axis), 0.0f);
		}
		return placement;
	}
}
