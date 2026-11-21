#include <danikk_space_engine/systems/chunk/chunk.h>
#include <danikk_space_engine/world.h>

#include <danikk_framework/glm.h>

namespace danikk_space_engine
{
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
