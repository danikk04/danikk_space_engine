#pragma once

#include <danikk_framework/virtual_dynamic_array.h>

#include <danikk_space_engine/default.h>
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

	class GameObject
	{
	private:
		DynamicArray<GameObject*> links;//Связанные объекты, не владеет ими
		DynamicArray<GameObject*> childs;//Владеет дочерними объектами
		GameObject* parent = nullptr;

	public:
		GameObject() = default;
		GameObject(const GameObject&) = delete;
		GameObject& operator=(const GameObject&) = delete;
		virtual ~GameObject();

		static GameObject* create();		
		void destroy();

		bool addChild(GameObject* child);
		bool addLink(GameObject* object);
		const DynamicArray<GameObject*>& getChilds() const { return childs; }
		const DynamicArray<GameObject*>& getLinks() const { return links; }

		//Кинетика
		vec3 speed = vec3(0.0f);
		vec3 rotation_speed = vec3(0.0f);

		//Расположение
		vec3 pos = vec3(0.0f);
		vec3 size = vec3(1.0f);
		vec3 rotation = vec3(0.0f);

		mat4 local_matrix = mat4(1.0f);
		mat4 world_matrix = mat4(1.0f);

		void tick();
		virtual void update();

		void frame();
		virtual void render();

		vec3 getFront()
		{
			return danikk_space_engine::getFront(rotation);
		}

		vec3 getUp()
		{
			mat4 roll_matrix = glm::rotate(mat4(1.0f), rotation.z, getFront());
			return vec3(roll_matrix * vec4(0.0f, 1.0f, 0.0f, 0.0f));
		}

		vec3 getRight()
		{
			return glm::normalize(glm::cross(getFront(), getUp()));
		}

		float getApproxRadius()
		{
			return 1.0f;
		}
	};
}
