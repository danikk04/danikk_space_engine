#pragma once

#include <default.h>
#include <object/object.h>
#include <object/world.h>

namespace danikk_space_engine
{
	class Camera : public WorldObject
	{
	public:
        void setViewMatrix();
	};
}
