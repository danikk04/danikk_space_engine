#pragma once

#include <object/object.h>

namespace game
{

	class WorldObject : public Object
	{
	public:
		vec3 pos;
		vec3 size = vec3(1.0f);
		vec3 rotation;

		mat4 local_matrix;
		mat4 world_matrix;

		void tick() override
		{
			mat4 local_matrix = mat4(1.0f);
			local_matrix = glm::translate(local_matrix, pos);
			local_matrix = glm::rotate(local_matrix, rotation.x, vec3(0, 1, 0));
			local_matrix = glm::rotate(local_matrix, rotation.y, vec3(1, 0, 0));
			WorldObject* parent_as_wo = dynamic_cast<WorldObject*>(object_stack.last());
			assert(parent_as_wo != NULL);
			world_matrix = parent_as_wo->world_matrix * (mat4)local_matrix;
			Object::tick();
			//rotation.y += 0.01f;
		}

		vec3 getFlatFront()
		{
			return vec3(
				(float)cos(rotation.x),
				0.0f,
				(float)sin(rotation.x)
			);
		}

		vec3 getFront()
		{
			return vec3(
				(float)cos(rotation.y) * (float)cos(rotation.x),
				(float)sin(rotation.y),
				(float)cos(rotation.y) * (float)sin(rotation.x)
			);
		}
	};
}
