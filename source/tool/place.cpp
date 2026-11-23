#include <danikk_space_engine/tool.h>
#include <danikk_space_engine/gui/gui.h>
#include <danikk_space_engine/world.h>
#include <danikk_space_engine/geometry/cube.h>
#include <danikk_space_engine/component/point/build_part.h>
#include <danikk_space_engine/systems/human/damageable.h>
#include <danikk_space_engine/systems/planet/system.h>

#include <cmath>

namespace danikk_space_engine
{
	namespace
	{

		/**
		 * @brief Attempts to create one BuildPart anchor and attach it to the hit part's WorldBody.
		 * @param world World receiving the point and component resources.
		 * @param hit Raycast result supplying placement position, normal, and optional parent part.
		 * A hit on terrain creates a new WorldBody with one BuildPart anchor point. A hit
		 * on a BuildPart adds a child part to that part's body and stores its slot as parent.
		 * Intersections with existing parts on that body are rejected before resources are allocated.
		 * @return True when the part was created; false when it would overlap another part.
		 */
		bool createBuildPartAtHit(
			World& world, const RaycastHit& hit, BuildPartShape shape, const vec3& size,
			uint8 rotation_step)
		{
			const BuildPartPlacement placement = getBuildPartPlacement(world, hit, shape, size, rotation_step);
			if(placement.body_id != static_cast<WorldBodyId>(-1)
				&& !world.physics.isBuildPartPlacementClear(placement.body_id, shape,
					placement.world_position, placement.body_local_rotation, size))
				return false;

			WorldBodyId body_id = placement.body_id;
			const PointComponentSlotId parent_slot_id = placement.parent_slot_id;
			const glm::dvec3 anchor_world_position = placement.world_position;

			if(body_id == static_cast<WorldBodyId>(-1))
			{
				body_id = world.world_body_data.createWorldBody();
				WorldBody& body = world.world_body_data.getWorldBody(body_id);
				body.faction_id = base_faction_ids::player;
				body.position = anchor_world_position;
				body.mass = 1.0;
				body.collision_radius = 0.0;
			}

			WorldBody& body = world.world_body_data.getWorldBody(body_id);
			const PointId point_id = world.point_data.createPoint();
			const glm::dquat inverse_body_rotation = glm::conjugate(body.rotation);
			const glm::dvec3 anchor_body_position = inverse_body_rotation
				* (anchor_world_position - body.position);

			BuildPartComponent part;
			part.shape = shape;
			part.size = size;
			part.parent_slot_id = parent_slot_id;
			part.rotation = placement.body_local_rotation;
			const PointComponentSlotId part_slot_id = world.point_data.createPointComponentSlot(
				point_id, PointComponentTypeId::build_part, &part);
			world.attachPointComponentToBody(body_id, point_id, part_slot_id, anchor_body_position);
			double shape_radius = 0.0;
			const BuildPartGeometry& geometry = getBuildPartGeometry(shape);
			for(const vec3& position : geometry.positions)
			{
				const vec3 scaled_position = position * size;
				const vec3 rotated_position = vec3(part.rotation * vec4(scaled_position, 0.0f));
				shape_radius = glm::max(shape_radius, static_cast<double>(glm::length(rotated_position)));
			}
			body.collision_radius = glm::max(body.collision_radius,
				glm::length(anchor_body_position) + shape_radius);
			if(parent_slot_id != no_build_part_parent) body.mass += 1.0;
			return true;
		}

	}

	ToolApplyResult applyCreateBodyTool(World& world, const RaycastHit& hit,
		const vec3& build_part_size, uint8 build_part_rotation_step)
	{
		if(!hit.hit) return ToolApplyResult();
		if(!createBuildPartAtHit(world, hit, BuildPartShape::cube,
			build_part_size, build_part_rotation_step)) gui.addMessage("Место занято");
		return ToolApplyResult();
	}

	ToolApplyResult applyCreateSlopeTool(World& world, const RaycastHit& hit,
		const vec3& build_part_size, uint8 build_part_rotation_step)
	{
		if(!hit.hit) return ToolApplyResult();
		if(!createBuildPartAtHit(world, hit, BuildPartShape::slope,
			build_part_size, build_part_rotation_step)) gui.addMessage("Место занято");
		return ToolApplyResult();
	}
}
