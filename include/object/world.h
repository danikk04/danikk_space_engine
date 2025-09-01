#pragma once

#include <object/object.h>

namespace danikk_space_engine
{

	class WorldObject : public virtual Object
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

	inline vec3 getFront(const vec3& rotation)
	{
		return vec3(
			(float)cos(rotation.y) * (float)cos(rotation.x),
			(float)sin(rotation.y),
			(float)cos(rotation.y) * (float)sin(rotation.x)
		);
	}
}
