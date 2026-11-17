#pragma once

#include <danikk_space_engine/fwd.h>
#include <danikk_space_engine/world/build_part_shape.h>

namespace danikk_space_engine
{

	/**
	 * @brief Draws a translucent blue BuildPart shape preview at a raycast hit.
	 * @param world World supplying the camera-relative transform and current camera position.
	 * @param hit Raycast result defining the placement point and outward normal.
	 * @param shape BuildPart primitive selected for placement.
	 * @param size Local dimensions in metres applied to the primitive.
	 * Does nothing on a miss. The shape is offset 0.03 m along the hit
	 * normal and rendered in camera-relative coordinates to retain precision at
	 * large world positions; it does not allocate a WorldBody or Chunk.
	 */
	void drawWorldBodyPlacementPreview(
		World& world, const RaycastHit& hit, BuildPartShape shape, const vec3& size);
}
