#pragma once

#include <default.h>
#include <object/gameobject.h>
#include <danikk_engine/mesh.h>

namespace danikk_space_engine
{
	class EntityObject : public GameObject
	{
	public:
		void frame()
		{
			GameObject::frame();
		}
	};
}
