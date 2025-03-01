#pragma once

#include <default.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace sotiria_project
{
	class Object
	{
	public:
		DynamicArray<Object*> childs;

		Object()
		{

		}

		virtual void tick(Object& parent)
		{
			for(Object* el : childs)
			{
				el->tick(*this);
			}
		}

		virtual void frame(Object& parent)
		{
			for(Object* el : childs)
			{
				el->frame(*this);
			}
		}
	};

	class WorldObject : virtual Object
	{
		vec3 pos;
		vec3 size;
		vec3 rotation;

		mat4 local_matrix;
		mat4 world_matrix;

		void tick(Object& parent) override
		{
			mat4 local_matrix = mat4(1.0f);
			local_matrix = glm::translate(local_matrix, pos);
			local_matrix = glm::rotate(local_matrix, 90.0f, rotation);
			WorldObject* parent_as_wo = dynamic_cast<WorldObject*>(&parent);
			assert(parent_as_wo != NULL);
			world_matrix = parent_as_wo->world_matrix * (mat4)local_matrix;
			Object::tick(parent);
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
