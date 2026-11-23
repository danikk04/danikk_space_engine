#include <danikk_space_engine/tool.h>
#include <danikk_space_engine/systems/world_body/system.h>
#include <danikk_space_engine/systems/build_object/system.h>
#include <danikk_space_engine/systems/build_part/system.h>
#include <danikk_space_engine/gui/gui.h>
#include <danikk_space_engine/world.h>
#include <danikk_space_engine/geometry/cube.h>
#include <danikk_space_engine/systems/human/damageable.h>
#include <danikk_space_engine/systems/planet/system.h>

#include <cmath>

#include <danikk_space_engine/systems/world_body/world_body.h>
#include <danikk_space_engine/systems/physics/physics.h>

#include <danikk_space_engine/systems/raycast/raycast.h>

namespace danikk_space_engine
{
	namespace
	{

		/**
		 * @brief Creates a BuildPart in the BuildObject selected by the placement result.
		 * @param world World receiving body, BuildObject, and BuildPart resources.
		 * @param hit Raycast result supplying placement position, normal, and optional reference part.
		 * A terrain hit creates a WorldBody and its BuildObject; a BuildPart hit adds an
		 * unparented part to the same BuildObject.
		 * Intersections with existing parts on that body are rejected before resources are allocated.
		 * @return True when the part was created; false when it would overlap another part.
		 */
		bool createBuildPartAtHit(
			World& world, const RaycastHit& hit, BuildPartShape shape, const vec3& size,
			uint8 rotation_step)
		{
			const BuildPartPlacement placement = getBuildPartPlacement(world, hit, shape, size, rotation_step);
			if(placement.body_id != static_cast<WorldBodyId>(-1)
				&& !world.getPhysicsSystem().isBuildPartPlacementClear(placement.body_id, shape,
					placement.world_position, placement.body_local_rotation, size))
				return false;

			WorldBodyId body_id = placement.body_id;
			BuildObjectId build_object_id = placement.build_object_id;
			const glm::dvec3 anchor_world_position = placement.world_position;

			if(body_id == static_cast<WorldBodyId>(-1))
			{
				body_id = world.getWorldBodySystem().createWorldBody();
				WorldBody& body = world.getWorldBodySystem().getWorldBody(body_id);
				body.faction_id = base_faction_ids::player;
				body.position = anchor_world_position;
				body.mass = 1.0;
				body.collision_radius = 0.0;
				build_object_id = world.getBuildObjectSystem().createBuildObject(body_id);
			}

			WorldBody& body = world.getWorldBodySystem().getWorldBody(body_id);
			if(!world.getBuildObjectSystem().isBuildObjectActive(build_object_id)) return false;
			const glm::dquat inverse_body_rotation = glm::conjugate(body.rotation);
			const glm::dvec3 body_local_position = inverse_body_rotation
				* (anchor_world_position - body.position);
			world.getBuildPartSystem().createBuildPart(build_object_id, shape,
				body_local_position, placement.body_local_rotation, size);
			double shape_radius = 0.0;
			const BuildPartGeometry& geometry = getBuildPartGeometry(shape);
			for(const vec3& position : geometry.positions)
			{
				const vec3 scaled_position = position * size;
				const vec3 rotated_position = vec3(placement.body_local_rotation * vec4(scaled_position, 0.0f));
				shape_radius = glm::max(shape_radius, static_cast<double>(glm::length(rotated_position)));
			}
			body.collision_radius = glm::max(body.collision_radius,
				glm::length(body_local_position) + shape_radius);
			if(placement.reference_build_part_id != static_cast<BuildPartId>(-1)) body.mass += 1.0;
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
