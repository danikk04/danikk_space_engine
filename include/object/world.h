#pragma once

#include <object/object.h>

namespace game
{

	class WorldObject : public Object
	{
	public:
		vec3 pos;
		vec3 size = vec3(1.0f);
		vec3 rotation;

		mat4 local_matrix;
		mat4 world_matrix;

		void tick() override;

		vec3 getFlatFront();

		vec3 getFront();
	};
}
