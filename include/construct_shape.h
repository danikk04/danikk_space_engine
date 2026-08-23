#pragma once

#include <danikk_engine/mesh.h>
#include <danikk_framework/dynamic_array.h>
#include <default.h>

namespace danikk_space_engine
{
	struct ConstructShape
	{
		Mesh mesh;
		vec3 size;
		float params[3];
	};

	DynamicArary<ConstructShape> construct_shapes;

	struct ConstructShapeType
	{
		static constexpr uint32 none = 0;
		static constexpr uint32 block = 1;

		uint32 data = 0;
	};
}
