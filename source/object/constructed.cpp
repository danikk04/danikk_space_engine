#include <danikk_space_engine/object/constructed.h>

#include <danikk_engine/texture_methods.h>
#include <danikk_framework/memory.h>

#include <new>

namespace danikk_space_engine
{
	ConstructedObject* ConstructedObject::create(ShapeId shape_id, vec3 size, vec4 color)
	{
		ShapeDefinition* shape = findShape(shape_id);
		if(shape == nullptr)
		{
			return nullptr;
		}

		void* memory = malloc(sizeof(ConstructedObject));
		if(memory == nullptr)
		{
			return nullptr;
		}
		ConstructedObject* object = new (memory) ConstructedObject();
		object->shape_size = size;
		object->draw_color = color;
		object->mesh = shape->getMesh(size);
		if(object->mesh == nullptr)
		{
			object->destroy();
			return nullptr;
		}
		return object;
	}

	void ConstructedObject::render()
	{
		setWorldMatrix(world_matrix);
		setDrawColor(draw_color);
		mesh->draw();
	}
}
