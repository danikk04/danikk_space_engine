#include <danikk_space_engine/systems/chunk/chunk.h>
#include <danikk_space_engine/world.h>

#include <danikk_framework/glm.h>

namespace danikk_space_engine
{
	/**
	 * @brief Transfers spatial metadata and mesh invalidation state from another chunk.
	 */
	Chunk::Chunk(Chunk&& other) noexcept
		: bounds_valid(other.bounds_valid),
		build_part_mesh_dirty(other.build_part_mesh_dirty),
		render_enabled(other.render_enabled),
		build_part_mesh_revision(other.build_part_mesh_revision),
		bounds_center(other.bounds_center), bounds_radius(other.bounds_radius),
		chunk_id(other.chunk_id), world_body_id(other.world_body_id),
		grid_coordinate(other.grid_coordinate), position(other.position)
	{
	}

	Chunk& Chunk::operator=(Chunk&& other) noexcept
	{
		if(this == &other)
		{
			return *this;
		}

		bounds_valid = other.bounds_valid;
		build_part_mesh_dirty = other.build_part_mesh_dirty;
		render_enabled = other.render_enabled;
		build_part_mesh_revision = other.build_part_mesh_revision;
		bounds_center = other.bounds_center;
		bounds_radius = other.bounds_radius;
		chunk_id = other.chunk_id;
		world_body_id = other.world_body_id;
		grid_coordinate = other.grid_coordinate;
		position = other.position;
		return *this;
	}

	/** @copydoc Chunk::invalidateBuildPartMesh */
	void Chunk::invalidateBuildPartMesh()
	{
		build_part_mesh_dirty = true;
		build_part_mesh_revision++;
		if(build_part_mesh_revision == 0) build_part_mesh_revision++;
	}

	/**
	 * @brief Combines body rotation and camera-relative chunk translation into the render transform.
	 * @param world World supplying the body's current transform and camera origin.
	 * @return Matrix mapping chunk-local vertices to camera-relative render coordinates.
	 */
	mat4 Chunk::createWorldMatrix(const World& world) const
	{
		const WorldBody& body = world.world_body_data.getWorldBody(world_body_id);
		const glm::dvec3 relative_chunk_position = body.position
			+ body.rotation * position
			- world.camera.getWorldPosition();
		mat4 body_rotation(1.0f);
		body_rotation[0] = vec4(vec3(body.rotation * glm::dvec3(1.0, 0.0, 0.0)), 0.0f);
		body_rotation[1] = vec4(vec3(body.rotation * glm::dvec3(0.0, 1.0, 0.0)), 0.0f);
		body_rotation[2] = vec4(vec3(body.rotation * glm::dvec3(0.0, 0.0, 1.0)), 0.0f);
		return glm::translate(mat4(1.0f), vec3(relative_chunk_position)) * body_rotation;
	}

}
