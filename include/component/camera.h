#pragma once

#include <default.h>
#include <component/component.h>
#include <component/world_object.h>
#include <danikk_framework/matrix.h>
#include <danikk_framework/glm.h>
#include <object.h>

namespace sotiria_project
{
	class CameraComponent : BaseComponent
	{
		mat4 view_matrix;
		mat4 projection_matrix;

		uint32 parent_world_object_component_index = 0;
	public:
		DEFINE_COMPONENT_ID
		DEFINE_COMPONENT_INIT_FUNCTION
		{
			projection_matrix = glm::perspective(60.0f, 16.0f / 10.0f, 1000.0f, -500.0f);
			parent_world_object_component_index = container.components.getComponentIndex<WorldObjectComponent>();
		}

		DEFINE_COMPONENT_TICK_FUNCTION
		{

		}

		DEFINE_COMPONENT_FRAME_FUNCTION
		{
			WorldObjectComponent* world_component = container.components.getComponent<WorldObjectComponent>(parent_world_object_component_index);

			view_matrix = glm::lookAt(world_component->pos, world_component->pos + world_component->getFront(), vec3(0.0f, 1.0f, 0.0f));
		}
	};
}
