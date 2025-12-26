#include <object/world.h>

namespace danikk_space_engine
{
	namespace object_tags
	{
		void World::tick()
		{
			mat4 local_matrix = mat4(1.0f);
			local_matrix = glm::translate(local_matrix, pos);
			local_matrix = glm::rotate(local_matrix, -rotation.x, vec3(0, 1, 0));
			local_matrix = glm::rotate(local_matrix, rotation.y, vec3(0, 0, 1));
			local_matrix = glm::scale(local_matrix, size);
			//Object* object = getCurrentObject();
			useParentObjectTag(World, world);
			if(world_tag != NULL)
			{
				world_matrix = world_tag->world_matrix * local_matrix;
			}
			else
			{
				world_matrix = local_matrix;
			}
			//rotation.y += 0.01f;
		}

		vec3 World::getFlatFront()
		{
			return vec3(
				(float)cos(rotation.x),
				0.0f,
				(float)sin(rotation.x)
			);
		}

		vec3 World::getFront()
		{
			return danikk_space_engine::getFront(rotation);
		}
	}
}
