#include <danikk_space_engine/tool.h>
#include <danikk_space_engine/world/world.h>
#include <danikk_space_engine/world/cube_mesh.h>
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
		 * @brief Creates one BuildPart anchor and attaches it to the hit part's WorldBody.
		 * @param world World receiving the point and component resources.
		 * @param hit Raycast result supplying placement position, normal, and optional parent part.
		 * A hit on terrain creates a new WorldBody with one chunk and anchor point. A hit
		 * on a BuildPart adds a child part to that part's body and stores its slot as parent.
		 */
		void createBuildPartAtHit(
			World& world, const RaycastHit& hit, BuildPartShape shape, const vec3& size)
		{
			constexpr double surface_offset = 0.03;
			vec3 up = hit.normal;
			if(glm::length(up) < 0.0001f) up = vec3(0.0f, 0.0f, 1.0f);
			up = glm::normalize(up);

			WorldBodyId body_id = static_cast<WorldBodyId>(-1);
			ChunkId chunk_id = static_cast<ChunkId>(-1);
			PointComponentSlotId parent_slot_id = no_build_part_parent;
			glm::dvec3 anchor_world_position = hit.position
				+ glm::dvec3(up) * (static_cast<double>(size.z) * 0.5 + surface_offset);
			if(hit.hit_build_part
				&& world.data.isPointComponentSlotActive(hit.build_part_slot_id))
			{
				const PointComponentSlot& parent_slot = world.data.getPointComponentSlot(hit.build_part_slot_id);
				if(parent_slot.type_id == PointComponentTypeId::build_part
					&& world.data.isChunkActive(parent_slot.chunk_id))
				{
					chunk_id = parent_slot.chunk_id;
					body_id = world.data.getChunk(chunk_id).world_body_id;
					parent_slot_id = hit.build_part_slot_id;
				}
			}

			if(body_id == static_cast<WorldBodyId>(-1))
			{
				body_id = world.data.createWorldBody();
				WorldBody& body = world.data.getWorldBody(body_id);
				body.faction_id = base_faction_ids::player;
				body.position = anchor_world_position;
				body.mass = 1.0;
				body.collision_radius = 0.0;
				Chunk& chunk = world.data.createChunk(body_id);
				chunk_id = chunk.chunk_id;
				anchor_world_position = body.position;
			}

			WorldBody& body = world.data.getWorldBody(body_id);
			Chunk& chunk = world.data.getChunk(chunk_id);
			const PointId point_id = world.data.createPoint();
			const glm::dquat inverse_body_rotation = glm::conjugate(body.rotation);
			const glm::dvec3 anchor_body_position = inverse_body_rotation
				* (anchor_world_position - body.position);
			world.data.getPoint(point_id).position = vec3(anchor_body_position - chunk.position);

			BuildPartComponent part;
			part.shape = shape;
			part.size = size;
			part.parent_slot_id = parent_slot_id;
			part.rotation = createBuildPartRotation(vec3(inverse_body_rotation * glm::dvec3(up)));
			const PointComponentSlotId part_slot_id = world.data.createPointComponentSlot(
				point_id, PointComponentTypeId::build_part, &part);
			world.data.addPointComponentSlotToChunk(chunk_id, part_slot_id);
			DamageableComponent damageable;
			damageable.definition_id = damageable_definition_ids::build_part;
			const float volume_factor = shape == BuildPartShape::slope ? 0.5f : 1.0f;
			damageable.setHealth(getDamageableDefinition(damageable.definition_id).max_health
				* size.x * size.y * size.z * volume_factor, point_id);
			const PointComponentSlotId damageable_id = world.data.createPointComponentSlot(
				point_id, PointComponentTypeId::damageable, &damageable);
			world.data.addPointComponentSlotToChunk(chunk_id, damageable_id);
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
			const PointId point_id = world.data.createPoint();
			Point& point = world.data.getPoint(point_id);
			const Chunk& chunk = world.data.getChunk(hit.chunk_id);
			const WorldBody& body = world.data.getWorldBody(chunk.world_body_id);
			const glm::dquat inverse_body_rotation = glm::conjugate(body.rotation);
			point.position = vec3(inverse_body_rotation
				* (hit.position + glm::dvec3(hit.normal * 0.03f)
				- world.getChunkWorldPosition(hit.chunk_id)));

			MachinegunComponent machinegun;
			machinegun.forward = vec3(inverse_body_rotation * glm::dvec3(world.getCameraComponent().getFront()));
			machinegun.up = vec3(inverse_body_rotation * glm::dvec3(world.getCameraComponent().getUp()));
			machinegun.right = vec3(inverse_body_rotation * glm::dvec3(world.getCameraComponent().getRight()));
			const PointComponentSlotId slot_id = world.data.createPointComponentSlot(
				point_id, PointComponentTypeId::machine_gun, &machinegun);
			world.data.addPointComponentSlotToChunk(hit.chunk_id, slot_id);
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
		World& world, const RaycastHit& hit, ToolActionId action, const vec3& build_part_size)
	{
		if(!hit.hit) return ToolApplyResult();
		switch(action)
		{
			case ToolActionId::machinegun:
				createMachinegunAtHit(world, hit);
				break;
			case ToolActionId::create_body:
				createBuildPartAtHit(world, hit, BuildPartShape::cube, build_part_size);
				break;
			case ToolActionId::create_slope:
				createBuildPartAtHit(world, hit, BuildPartShape::slope, build_part_size);
				break;
			case ToolActionId::none:
			case ToolActionId::extrude:
			case ToolActionId::erase:
			case ToolActionId::drag:
			case ToolActionId::create_human:
			case ToolActionId::raid:
			case ToolActionId::count:
				break;
		}
		return ToolApplyResult();
	}
}
