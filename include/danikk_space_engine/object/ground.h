#pragma once

#include <danikk_space_engine/default.h>
#include <danikk_space_engine/object/gameobject.h>
#include <danikk_engine/mesh.h>

namespace danikk_space_engine
{
	class GroundObject : public GameObject
	{
	public:
		float gravity_acceleration = 9.8f;
		float friction_acceleration = 0.01f;

		static GroundObject* create();

		void generateLevel()
		{

		}

		void update() override;
	};
}
