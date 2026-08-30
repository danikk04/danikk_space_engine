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

	ConstructShape* getConstructShape(int32 type, vec3 size, float* params);

	void initConstructShapeTypes();

	enum ConstructShapeTypes
	{
		none,
		block
	};
}
