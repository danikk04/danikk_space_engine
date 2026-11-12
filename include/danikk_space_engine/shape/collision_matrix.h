#pragma once

#include <danikk_space_engine/shape/shape.h>
#include <danikk_space_engine/fwd.h>

namespace danikk_space_engine
{
	using CollisionFunction = CollisionInfo (*)(const GameObject& object_a, const GameObject& object_b);

	extern const CollisionFunction collision_matrix[ShapeType::count][ShapeType::count];
}
