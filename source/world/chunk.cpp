#include <danikk_space_engine/profiler.h>
#include <danikk_space_engine/world/chunk.h>
#include <danikk_space_engine/world/world.h>

#include <danikk_framework/glm.h>

namespace danikk_space_engine
{
	namespace
	{
		/** @brief Removes an ID from a callback list while preserving traversal order. */
		bool removePointComponentId(DynamicArray<PointComponentSlotId>& ids, PointComponentSlotId slot_id)
		{
			for(size_t end = ids.size(); end > 0; --end)
			{
				if(ids[end - 1] != slot_id) continue;
				for(size_t index = end; index < ids.size(); ++index) ids[index - 1] = ids[index];
				ids.pop();
				return true;
			}
			return false;
		}
	}

	/**
	 * @brief Transfers mesh and resource-array ownership from another chunk.
	 */
	Chunk::Chunk(Chunk&& other) noexcept
		: bounds_valid(other.bounds_valid),
		planet_terrain(other.planet_terrain),
		human_component_slot_ids(static_cast<DynamicArray<PointComponentSlotId>&&>(other.human_component_slot_ids)),
		has_active_human_collisions(other.has_active_human_collisions),
		build_part_mesh_dirty(other.build_part_mesh_dirty),
		build_part_mesh_revision(other.build_part_mesh_revision),
		bounds_center(other.bounds_center), bounds_radius(other.bounds_radius),
		chunk_id(other.chunk_id), world_body_id(other.world_body_id),
		grid_coordinate(other.grid_coordinate), position(other.position),
		point_ids(static_cast<DynamicArray<PointId>&&>(other.point_ids)),
		point_component_slot_ids(static_cast<DynamicArray<PointComponentSlotId>&&>(other.point_component_slot_ids))
	{
	}

	Chunk& Chunk::operator=(Chunk&& other) noexcept
	{
		if(this == &other)
		{
			return *this;
		}

		has_active_human_collisions = other.has_active_human_collisions;
		bounds_valid = other.bounds_valid;
		planet_terrain = other.planet_terrain;
		build_part_mesh_dirty = other.build_part_mesh_dirty;
		build_part_mesh_revision = other.build_part_mesh_revision;
		bounds_center = other.bounds_center;
		bounds_radius = other.bounds_radius;
		chunk_id = other.chunk_id;
		world_body_id = other.world_body_id;
		grid_coordinate = other.grid_coordinate;
		position = other.position;
		point_ids = static_cast<DynamicArray<PointId>&&>(other.point_ids);
		point_component_slot_ids = static_cast<DynamicArray<PointComponentSlotId>&&>(other.point_component_slot_ids);
		human_component_slot_ids = static_cast<DynamicArray<PointComponentSlotId>&&>(other.human_component_slot_ids);
		return *this;
	}

	/** @copydoc Chunk::invalidateBuildPartMesh */
	void Chunk::invalidateBuildPartMesh()
	{
		build_part_mesh_dirty = true;
		build_part_mesh_revision++;
		if(build_part_mesh_revision == 0) build_part_mesh_revision++;
	}

	/** @copydoc Chunk::addPointComponentSlot */
	void Chunk::addPointComponentSlot(PointComponentSlotId slot_id, PointComponentTypeId type_id)
	{
		point_component_slot_ids.push(slot_id);
		if(type_id == PointComponentTypeId::human) human_component_slot_ids.push(slot_id);
	}

	/** @copydoc Chunk::removePointComponentSlot */
	void Chunk::removePointComponentSlot(PointComponentSlotId slot_id, PointComponentTypeId type_id)
	{
		if(!removePointComponentId(point_component_slot_ids, slot_id))
			danikk_engine::formatFatalError("Point component % is missing from owning chunk %", slot_id, chunk_id);
		if(type_id == PointComponentTypeId::human && !removePointComponentId(human_component_slot_ids, slot_id))
			danikk_engine::formatFatalError("Human component % is missing from chunk % cache", slot_id, chunk_id);
	}

	/**
	 * @brief Combines body rotation and camera-relative chunk translation into the render transform.
	 * @param world World supplying the body's current transform and camera origin.
	 * @return Matrix mapping chunk-local vertices to camera-relative render coordinates.
	 */
	mat4 Chunk::createWorldMatrix(const World& world) const
	{
		const WorldBody& body = world.data.getWorldBody(world_body_id);
		const glm::dvec3 relative_chunk_position = body.position
			+ body.rotation * position
			- world.getCameraWorldPosition();
		mat4 body_rotation(1.0f);
		body_rotation[0] = vec4(vec3(body.rotation * glm::dvec3(1.0, 0.0, 0.0)), 0.0f);
		body_rotation[1] = vec4(vec3(body.rotation * glm::dvec3(0.0, 1.0, 0.0)), 0.0f);
		body_rotation[2] = vec4(vec3(body.rotation * glm::dvec3(0.0, 0.0, 1.0)), 0.0f);
		return glm::translate(mat4(1.0f), vec3(relative_chunk_position)) * body_rotation;
	}

}
