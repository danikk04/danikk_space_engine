#include <object/gameobject.h>
#include <manager.h>

namespace danikk_space_engine
{
	void GameObject::tick()
	{
		//Расположение
		mat4 local_matrix = mat4(1.0f);
		local_matrix = glm::translate(local_matrix, pos);
		local_matrix = glm::rotate(local_matrix, -rotation.x, vec3(0, 1, 0));
		local_matrix = glm::rotate(local_matrix, rotation.y, vec3(0, 0, 1));
		local_matrix = glm::scale(local_matrix, size);
		GameObject* parent = (GameObject*)getParent();
		world_matrix = parent->world_matrix * local_matrix;

		//Кинетика
		pos += speed * getTargetFrameDelay();
		rotation += rotation_speed * getTargetFrameDelay();

		pushParent(this);
		for(GameObject* child : childs)
		{
			child->tick();
		}
		popParent();
	}

	void GameObject::frame()
	{
		pushParent(this);
		popParent();
	}
}
