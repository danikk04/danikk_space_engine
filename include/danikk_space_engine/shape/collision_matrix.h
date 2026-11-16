#pragma once

#include <danikk_space_engine/shape/shape.h>
#include <danikk_space_engine/fwd.h>

namespace danikk_space_engine
{
	/**
	 * @brief Defines the function signature used by each shape-pair collision test.
	 */
	using CollisionFunction = CollisionInfo (*)(const GameObject& object_a, const GameObject& object_b);

	/**
	 * @brief Selects the collision test for each ordered pair of shape types.
	 */
	extern const CollisionFunction collision_matrix[ShapeType::count][ShapeType::count];
}
