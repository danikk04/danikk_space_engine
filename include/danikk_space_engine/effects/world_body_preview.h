#pragma once

#include <danikk_space_engine/fwd.h>

namespace danikk_space_engine
{

	/**
	 * @brief Draws a translucent blue one-metre cube preview above a raycast hit.
	 * @param world World supplying the camera-relative transform and current camera position.
	 * @param hit Raycast result defining the placement point and outward normal.
	 * Does nothing on a miss. The preview cube is offset 0.03 m along the hit
	 * normal and rendered in camera-relative coordinates to retain precision at
	 * large world positions; it does not allocate a WorldBody or Chunk.
	 */
	void drawWorldBodyPlacementPreview(World& world, const RaycastHit& hit);
}
