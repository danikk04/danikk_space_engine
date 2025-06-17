#pragma once

#include <default.h>
#include <object/object.h>
#include <object/world.h>

namespace game
{
	class Camera : public WorldObject
	{
	public:
        void setViewMatrix();
	};
}
