#include <danikk_space_engine/object/gameobject.h>
#include <danikk_space_engine/manager.h>

namespace danikk_space_engine
{
	GameObject::~GameObject()
	{
		for(GameObject* child : childs)
		{
			delete child;
		}
	}

	bool GameObject::addChild(GameObject* child)
	{
		if(child == nullptr || child == this || child->parent != nullptr)
		{
			return false;
		}
		for(GameObject* ancestor = this; ancestor != nullptr; ancestor = ancestor->parent)
		{
			if(ancestor == child)
			{
				return false;
			}
		}
		child->parent = this;
		childs.push(child);
		return true;
	}

	bool GameObject::addLink(GameObject* object)
	{
		if(object == nullptr || object == this)
		{
			return false;
		}
		for(GameObject* link : links)
		{
			if(link == object)
			{
				return false;
			}
		}
		links.push(object);
		return true;
	}

	void GameObject::tick()
	{
		update();

		//Сначала обновляем локальное положение, затем считаем матрицы.
		pos += speed * getTargetFrameDelay();
		rotation += rotation_speed * getTargetFrameDelay();

		local_matrix = mat4(1.0f);
		local_matrix = glm::translate(local_matrix, pos);
		local_matrix = glm::rotate(local_matrix, -rotation.x, vec3(0, 1, 0));
		local_matrix = glm::rotate(local_matrix, rotation.y, vec3(0, 0, 1));
		local_matrix = glm::scale(local_matrix, size);
		world_matrix = parent == nullptr ? local_matrix : parent->world_matrix * local_matrix;

		for(GameObject* child : childs)
		{
			child->tick();
		}
	}

	void GameObject::frame()
	{
		render();
		for(GameObject* child : childs)
		{
			child->frame();
		}
	}

	void GameObject::update() {}

	void GameObject::render() {}
}
