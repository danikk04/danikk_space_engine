#pragma once

#include <default.h>
#include <object/object.h>
#include <object/meshed.h>
#include <object/physic.h>
#include <danikk_engine/danikk_engine.h>
#include <danikk_engine/built_in_meshes.h>
#include <asset.h>

namespace game
{
	class Container : public MeshedObject, public PhysicObject
	{
	public:
		Container() : WorldObject(), MeshedObject(), PhysicObject()
		{
			mesh = &cube_mesh;
			texture = static_asset_collection.container_texture;
		}

		void frame() override
		{
			MeshedObject::frame();
		}

		void tick() override
		{
			WorldObject::tick();
			PhysicObject::tick();
			//world_matrix = mat4(1.0f);
		}
	};
}
