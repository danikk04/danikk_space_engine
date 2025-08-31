#include <object/world.h>

namespace danikk_space_engine
{
	void WorldObject::tick()
	{
		mat4 local_matrix = mat4(1.0f);
		local_matrix = glm::translate(local_matrix, pos);
		local_matrix = glm::rotate(local_matrix, -rotation.x, vec3(0, 1, 0));
		local_matrix = glm::rotate(local_matrix, rotation.y, vec3(0, 0, 1));
		local_matrix = glm::scale(local_matrix, size);
		WorldObject* parent_as_wo = dynamic_cast<WorldObject*>(object_stack.last());
		assert(parent_as_wo != NULL);
		world_matrix = parent_as_wo->world_matrix * (mat4)local_matrix;
		Object::tick();
		//rotation.y += 0.01f;
	}

	vec3 WorldObject::getFlatFront()
	{
		return vec3(
			(float)cos(rotation.x),
			0.0f,
			(float)sin(rotation.x)
		);
	}

	vec3 WorldObject::getFront()
	{
		return danikk_space_engine::getFront(rotation);
	}
}
