#pragma once

#include <danikk_space_engine/default.h>
#include <danikk_space_engine/object/gameobject.h>
#include <danikk_space_engine/shape/shape.h>
#include <danikk_engine/mesh.h>

namespace danikk_space_engine
{
	class ConstructedObject : public GameObject
	{
	private:
		ShapeMesh* mesh_entry = nullptr; // Borrowed from the shape mesh cache.
		ShapeDefinition* shape_definition = nullptr;

		ConstructedObject() = default;

	public:
		static ConstructedObject* create(ShapeId shape_id, vec3 size);

		const ShapeDefinition* getShapeDefinition() const override;
		void render() override;
	};
}
