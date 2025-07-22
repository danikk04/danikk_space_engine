#pragma once

#include <default.h>
#include <object/object.h>
#include <object/meshed.h>
#include <object/physic.h>
#include <danikk_engine/danikk_engine.h>
#include <danikk_engine/built_in_meshes.h>
#include <danikk_engine/texture.h>
#include <asset.h>

namespace danikk_space_engine
{
	class Container : public MeshedObject, public PhysicObject
	{
	public:
		Container() : WorldObject(), MeshedObject(), PhysicObject()
		{
			mesh = &cube_mesh;
			texture = Texture("container");
		}

		void frame() override
		{
			MeshedObject::frame();
		}

		void tick() override
		{
			WorldObject::tick();
			PhysicObject::tick();
		}
	};
}
