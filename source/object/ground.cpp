#include <danikk_space_engine/object/ground.h>
#include <danikk_framework/memory.h>

#include <new>
#include <cstdlib>

namespace danikk_space_engine
{
	GroundObject* GroundObject::create()
	{
		void* memory = malloc(sizeof(GroundObject));
		if(memory == nullptr) std::abort();
		return new (memory) GroundObject();
	}

	void GroundObject::update()
	{
		const float delta_time = getTargetFrameDelay();
		for(GameObject* child : getChilds())
		{
			const float distance = glm::length(child->pos);
			if(distance > 0.0001f)
			{
				const vec3 gravity_direction = -child->pos / distance;
				child->speed += gravity_direction * gravity_acceleration * delta_time;
			}

			const float speed = glm::length(child->speed);
			if(speed <= 0.0001f)
			{
				child->speed = vec3(0.0f);
				continue;
			}

			const float speed_loss = friction_acceleration * delta_time;
			if(speed_loss >= speed)
			{
				child->speed = vec3(0.0f);
			}
			else
			{
				child->speed -= child->speed / speed * speed_loss;
			}
		}
	}
}
