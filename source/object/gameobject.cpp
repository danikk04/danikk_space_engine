#include <danikk_space_engine/object/gameobject.h>
#include <danikk_space_engine/shape/collision_matrix.h>
#include <danikk_framework/memory.h>

#include <new>
#include <cstdlib>

namespace danikk_space_engine
{
	GameObject* GameObject::create()
	{
		void* memory = malloc(sizeof(GameObject));
		if(memory == nullptr) std::abort();
		return new (memory) GameObject();
	}

	void GameObject::destroy()
	{
		this->~GameObject();
		free(this);
	}

	GameObject::~GameObject()
	{
		for(GameObject* child : childs)
		{
			child->destroy();
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
		updateTransform();

		for(GameObject* child : childs)
		{
			child->tick();
		}

		for(index_t a = 0; a < childs.size(); ++a)
		{
			for(index_t b = a + 1; b < childs.size(); ++b)
			{
				GameObject* object_a = childs[a];
				GameObject* object_b = childs[b];
				const ShapeDefinition* shape_a = object_a->getShapeDefinition();
				const ShapeDefinition* shape_b = object_b->getShapeDefinition();
				if(shape_a == nullptr || shape_b == nullptr ||
						shape_a->id >= ShapeType::count || shape_b->id >= ShapeType::count)
				{
					continue;
				}

				const vec3 size_a = glm::abs(object_a->size);
				const vec3 size_b = glm::abs(object_b->size);
				const vec3 position_delta = object_b->pos - object_a->pos;
				const float size_bound_squared = 0.5f *
						(glm::dot(size_a, size_a) + glm::dot(size_b, size_b));
				if(glm::dot(position_delta, position_delta) > size_bound_squared)
				{
					continue;
				}

				CollisionInfo collision = collision_matrix[shape_a->id][shape_b->id](*object_a, *object_b);
				if(!collision.collided)
				{
					continue;
				}

				const vec3 correction = collision.normal * collision.penetration;
				object_a->pos -= correction * 0.5f;
				object_b->pos += correction * 0.5f;

				const float closing_speed = glm::dot(object_b->speed - object_a->speed, collision.normal);
				if(closing_speed < 0.0f)
				{
					const vec3 velocity_impulse = collision.normal * (-closing_speed * 0.5f);
					object_a->speed -= velocity_impulse;
					object_b->speed += velocity_impulse;
				}

				object_a->updateTransform();
				object_b->updateTransform();
			}
		}
	}

	void GameObject::updateTransform()
	{
		local_matrix = mat4(1.0f);
		local_matrix = glm::translate(local_matrix, pos);
		local_matrix = glm::rotate(local_matrix, -rotation.x, vec3(0, 1, 0));
		local_matrix = glm::rotate(local_matrix, rotation.y, vec3(0, 0, 1));
		local_matrix = glm::scale(local_matrix, size);
		world_matrix = parent == nullptr ? local_matrix : parent->world_matrix * local_matrix;
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
