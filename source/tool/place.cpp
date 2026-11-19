#include <danikk_space_engine/tool.h>
#include <danikk_space_engine/gui.h>
#include <danikk_space_engine/world/world.h>
#include <danikk_space_engine/geometry/cube.h>
#include <danikk_space_engine/component/point/machinegun.h>
#include <danikk_space_engine/component/point/build_part.h>
#include <danikk_space_engine/component/point/damageable.h>
#include <danikk_space_engine/component/world_body/planet/planet.h>

#include <cmath>

namespace danikk_space_engine
{
	namespace
	{

		/**
		 * @brief Attempts to create one BuildPart anchor and attach it to the hit part's WorldBody.
		 * @param world World receiving the point and component resources.
		 * @param hit Raycast result supplying placement position, normal, and optional parent part.
		 * A hit on terrain creates a new WorldBody with one chunk and anchor point. A hit
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
			ChunkId chunk_id = placement.chunk_id;
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
				Chunk& chunk = world.chunks.createChunk(body_id);
				chunk_id = chunk.chunk_id;
			}

			WorldBody& body = world.world_body_data.getWorldBody(body_id);
			Chunk& chunk = world.chunks.getChunk(chunk_id);
			const PointId point_id = world.point_data.createPoint();
			const glm::dquat inverse_body_rotation = glm::conjugate(body.rotation);
			const glm::dvec3 anchor_body_position = inverse_body_rotation
				* (anchor_world_position - body.position);
			world.point_data.getPoint(point_id).position = vec3(anchor_body_position - chunk.position);

			BuildPartComponent part;
			part.shape = shape;
			part.size = size;
			part.parent_slot_id = parent_slot_id;
			part.rotation = placement.body_local_rotation;
			const PointComponentSlotId part_slot_id = world.point_data.createPointComponentSlot(
				point_id, PointComponentTypeId::build_part, &part);
			world.chunks.addPointComponentSlotToChunk(chunk_id, part_slot_id);
			DamageableComponent damageable;
			damageable.definition_id = damageable_definition_ids::build_part;
			const float volume_factor = shape == BuildPartShape::slope ? 0.5f : 1.0f;
			damageable.setHealth(getDamageableDefinition(damageable.definition_id).max_health
				* size.x * size.y * size.z * volume_factor, point_id);
			const PointComponentSlotId damageable_id = world.point_data.createPointComponentSlot(
				point_id, PointComponentTypeId::damageable, &damageable);
			world.chunks.addPointComponentSlotToChunk(chunk_id, damageable_id);
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

		/**
		 * @brief Creates a point-attached machine gun in the hit chunk, oriented to the camera.
		 * @param world World receiving the point and machine-gun component slot.
		 * @param hit Surface hit used for the local point position and owning chunk.
		 * The point is offset 0.03 m from the surface, transformed into chunk-local
		 * coordinates; the weapon basis is transformed from camera axes into the
		 * owning WorldBody's local orientation.
		 */
		void createMachinegunAtHit(World& world, const RaycastHit& hit)
		{
			const PointId point_id = world.point_data.createPoint();
			Point& point = world.point_data.getPoint(point_id);
			const Chunk& chunk = world.chunks.getChunk(hit.chunk_id);
			const WorldBody& body = world.world_body_data.getWorldBody(chunk.world_body_id);
			const glm::dquat inverse_body_rotation = glm::conjugate(body.rotation);
			point.position = vec3(inverse_body_rotation
				* (hit.position + glm::dvec3(hit.normal * 0.03f)
				- world.getChunkWorldPosition(hit.chunk_id)));

			MachinegunComponent machinegun;
			machinegun.forward = vec3(inverse_body_rotation * glm::dvec3(world.getCameraComponent().getFront()));
			machinegun.up = vec3(inverse_body_rotation * glm::dvec3(world.getCameraComponent().getUp()));
			machinegun.right = vec3(inverse_body_rotation * glm::dvec3(world.getCameraComponent().getRight()));
			const PointComponentSlotId slot_id = world.point_data.createPointComponentSlot(
				point_id, PointComponentTypeId::machine_gun, &machinegun);
			world.chunks.addPointComponentSlotToChunk(hit.chunk_id, slot_id);
		}

	}

	/**
	 * @brief Dispatches placement of a machine gun or a cube/slope BuildPart at a valid ray hit.
	 * @param world World receiving the created resources.
	 * @param hit Raycast result defining the target chunk, point, and normal.
	 * @param action Place action; unsupported values are ignored.
	 * @return Empty selection result; a miss leaves the world unchanged.
	 */
	ToolApplyResult applyPlaceTool(
		World& world, const RaycastHit& hit, ToolActionId action, const vec3& build_part_size,
		uint8 build_part_rotation_step)
	{
		if(!hit.hit) return ToolApplyResult();
		switch(action)
		{
			case ToolActionId::machinegun:
				createMachinegunAtHit(world, hit);
				break;
			case ToolActionId::create_body:
				if(!createBuildPartAtHit(world, hit, BuildPartShape::cube,
					build_part_size, build_part_rotation_step))
					gui.addMessage("Место занято");
				break;
			case ToolActionId::create_slope:
				if(!createBuildPartAtHit(world, hit, BuildPartShape::slope,
					build_part_size, build_part_rotation_step))
					gui.addMessage("Место занято");
				break;
			case ToolActionId::none:
			case ToolActionId::extrude:
			case ToolActionId::erase:
			case ToolActionId::drag:
			case ToolActionId::create_human:
			case ToolActionId::create_reimu:
			case ToolActionId::create_marisa:
			case ToolActionId::create_tsumi_bukuro:
			case ToolActionId::spawn_5_allies:
			case ToolActionId::spawn_5_pirates:
			case ToolActionId::count:
				break;
		}
		return ToolApplyResult();
	}
}
