#pragma once

#include <danikk_space_engine/fwd.h>
#include <danikk_space_engine/geometry/build_part.h>

namespace danikk_space_engine
{

	/**
	 * @brief Draws a translucent blue BuildPart shape preview at a raycast hit.
	 * @param world World supplying the camera-relative transform and current camera position.
	 * @param hit Raycast result defining the placement point and outward normal.
	 * @param shape BuildPart primitive selected for placement.
	 * @param size Local dimensions in metres applied to the primitive.
	 * @param axis Selected local size axis: 0 for X, 1 for Y, 2 for Z.
	 * @param rotation_step Snapped quarter-turn angle around the placement normal.
	 * Does nothing on a miss. The shared placement transform is rendered in camera-relative
	 * coordinates to retain precision at large world positions; no WorldBody or point is allocated.
	 */
	void drawWorldBodyPlacementPreview(
		World& world, const RaycastHit& hit, BuildPartShape shape, const vec3& size,
		uint8 axis, uint8 rotation_step);
}
