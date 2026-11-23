#pragma once

#include <danikk_space_engine/id.h>
#include <danikk_space_engine/geometry/build_part.h>

namespace danikk_space_engine
{
	/** @brief One independently stored structural part belonging to a BuildObject. */
	struct BuildPart
	{
		BuildObjectId build_object_id = static_cast<BuildObjectId>(-1);
		BuildPartShape shape = BuildPartShape::cube;
		/** @brief Position in the owning BuildObject's WorldBody-local frame. */
		glm::dvec3 body_local_position = glm::dvec3(0.0);
		/** @brief Orientation in the owning WorldBody's local frame. */
		mat4 rotation = mat4(1.0f);
		vec3 size = vec3(1.0f);
		float construction_progress = 1.0f;
	};
}
