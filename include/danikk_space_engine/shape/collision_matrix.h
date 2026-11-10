#pragma once

#include <danikk_space_engine/shape/shape.h>

namespace danikk_space_engine
{
	class GameObject;

	using CollisionFunction = CollisionInfo (*)(const GameObject& object_a, const GameObject& object_b);

	extern const CollisionFunction collision_matrix[ShapeType::count][ShapeType::count];
}
