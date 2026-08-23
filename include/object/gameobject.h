#pragma once

#include <danikk_framework/virtual_dynamic_array.h>

#include <default.h>
#include <danikk_framework/glm.h>
#include <danikk_framework/iterable.h>
#include <danikk_framework/array.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace danikk_space_engine
{
	inline vec3 getFront(const vec3& rotation)
	{
		return vec3(
				(float)cos(rotation.y) * (float)cos(rotation.x),
				(float)sin(rotation.y),
				(float)cos(rotation.y) * (float)sin(rotation.x)
		);
	}

	//TODO: сделать так, чтобы массивы и кинетика были в отдельных структурах, которые будут выделяться в памяти по необходимости
	class GameObject
	{
	public:
		DynamicArray<GameObject*> links;//Связанные объекты (стыковка и т.п.)
		DynamicArray<GameObject*> childs;//Дочерние объекты (блоки в структуре и т.п.)

		//Кинетика
		vec3 speed;
		vec3 rotation_speed;

		//Расположение
		vec3 pos;
		vec3 size = vec3(1.0f);
		vec3 rotation;

		mat4 local_matrix;
		mat4 world_matrix;
		virtual void tick();

		virtual void frame();

		vec3 getFront()
		{
			return danikk_space_engine::getFront(rotation);
		}

		float getApproxRadius()
		{
			return 1.0f;
		}
	};
}
