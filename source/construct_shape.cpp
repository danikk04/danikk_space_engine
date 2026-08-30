#pragma once

#include <construct_shape.h>
#include <danikk_engine/dynamic_mesh.h>

namespace danikk_space_engine
{
	typedef void (*InitConstructShapeF)(ConstructShape*);

	struct ConstructShapeType
	{
		DynamicArray<ConstructShape> construct_shapes;

		InitConstructShapeF init_func;
	};

	struct ConstructShapeTypeContainer
	{
		DynamicArray<ConstructShapeType> construct_shape_types;

		void addType(InitConstructShapeF init_func)
		{
			construct_shape_types.pushCtor().init_func = init_func;
		}
	};

	ConstructShapeTypeContainer construct_shape_type_container;

	void InitNoneShape(ConstructShape* shape)
	{
		assert(false);
	}

	inline const ivec3 block_directions[6]
	{
		ivec3(1, 0, 0),
		ivec3(0, 1, 0),
		ivec3(0, 0, 1),

		ivec3(-1, 0, 0),
		ivec3(0, -1, 0),
		ivec3(0, 0, -1),
	};

	void InitBlockShape(ConstructShape* shape)
	{
		DynamicMesh<DefaultVertex>  dynamic_mesh;

		for(const ivec3& direction : block_directions)
		{
			ivec3 directed_pos = ivec3(pos + direction);
			vec3 offseted = vec3(0.5f) + vec3(direction) / 2.0f + vec3(block_global_offset);

			dynamic_mesh.addSquare(offseted, vec3(direction));
		}
	}

	ConstructShape* getConstructShape(int32 type, vec3 size, float* params)
	{

	}

	void initConstructShapeTypes()
	{
		construct_shape_type_container.addType(InitNoneShape);
		construct_shape_type_container.addType(InitBlockShape);
	}
}
