#include <danikk_space_engine/object/constructed.h>

#include <danikk_engine/texture_methods.h>
#include <danikk_space_engine/asset.h>
#include <danikk_framework/memory.h>

#include <new>
#include <cstdlib>

namespace danikk_space_engine
{
	ConstructedObject* ConstructedObject::create(ShapeId shape_id, vec3 size)
	{
		ShapeDefinition* shape = findShape(shape_id);
		if(shape == nullptr) std::abort();

		void* memory = malloc(sizeof(ConstructedObject));
		if(memory == nullptr) std::abort();
		ConstructedObject* object = new (memory) ConstructedObject();
		object->size = size;
		object->shape_definition = shape;
		object->mesh_entry = shape->getMeshEntry(size);
		if(object->mesh_entry == nullptr) std::abort();
		return object;
	}

	void ConstructedObject::render()
	{
		setWorldMatrix(world_matrix);
		setDrawColor(vec4(1.0f));
		static_asset_collection.container_texture.bind();
		mesh_entry->mesh->draw();
	}

	const ShapeDefinition* ConstructedObject::getShapeDefinition() const
	{
		return shape_definition;
	}
}
