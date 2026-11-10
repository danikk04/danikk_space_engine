#pragma once

#include <danikk_space_engine/default.h>
#include <danikk_space_engine/object/gameobject.h>
#include <danikk_space_engine/shape.h>
#include <danikk_engine/mesh.h>

namespace danikk_space_engine
{
	class ConstructedObject : public GameObject
	{
	private:
		Mesh* mesh = nullptr; // Borrowed from the shape mesh cache.
		vec3 shape_size = vec3(1.0f);

		ConstructedObject() = default;

	public:
		static ConstructedObject* create(ShapeId shape_id, vec3 size);

		void render() override;
	};
}
