#pragma once

#include <default.h>
#include <component/component.h>
#include <danikk_engine/mesh.h>
#include <object.h>

namespace sotiria_project
{
	class MeshDrawComponent : BaseComponent
	{
	public:
		Mesh mesh;
		Texture texture;

		DEFINE_COMPONENT_ID

		DEFINE_COMPONENT_INIT_FUNCTION
		{

		}

		DEFINE_COMPONENT_FRAME_FUNCTION
		{

		}

		DEFINE_COMPONENT_TICK_FUNCTION
		{

		}
	};
}
