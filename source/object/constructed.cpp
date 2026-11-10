#include <danikk_space_engine/object/constructed.h>

#include <danikk_engine/texture_methods.h>
#include <danikk_space_engine/asset.h>
#include <danikk_framework/memory.h>

#include <new>

namespace danikk_space_engine
{
	ConstructedObject* ConstructedObject::create(ShapeId shape_id, vec3 size)
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
		setDrawColor(vec4(1.0f));
		static_asset_collection.container_texture.bind();
		mesh->draw();
	}
}
