#pragma once

#include <default.h>
#include <danikk_engine/voxel_map.h>

namespace sotiria_project
{
	class WorldObject
	{
	public:
		vec3 pos;
		vec3 size;
		vec3 rotation;

		mat4 world_matrix;

		uint32 parent_world_object_component_index;

		void tick()
		{
			mat4 pos_matrix = mat4(1.0f);
			pos_matrix = glm::translate(pos_matrix, pos);
			pos_matrix = glm::rotate(pos_matrix, 90.0f, rotation);
			//world_matrix = world_component->world_matrix * (mat4)pos_matrix;
		}

		void frame()
		{

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
