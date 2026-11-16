#pragma once

#include <danikk_space_engine/shape/shape.h>
#include <danikk_space_engine/fwd.h>

namespace danikk_space_engine
{
	/**
	 * @brief Signature of a legacy collision test for an ordered pair of GameObjects.
	 * The returned normal is oriented from the first object toward the second as
	 * defined by each matrix entry. This dispatch API is disconnected from active physics.
	 */
	using CollisionFunction = CollisionInfo (*)(const GameObject& object_a, const GameObject& object_b);

	/**
	 * @brief Maps each ordered pair of legacy ShapeType IDs to its collision function.
	 * Rows correspond to the first object; columns correspond to the second.
	 * Entries involving none dispatch to the no-collision function.
	 */
	extern const CollisionFunction collision_matrix[ShapeType::count][ShapeType::count];
}
