#pragma once

#include <danikk_space_engine/fwd.h>

namespace danikk_space_engine
{

	/**
	 * @brief Draws black wire-like box edges around chunks with drawable geometry.
	 * @param world World whose currently stored body/chunk geometry is inspected.
	 * A chunk with any ground triangles currently uses a box from local origin to
	 * `planet_chunk_width` on all axes; a chunk without triangles uses the axis-aligned
	 * bounds of its active referenced points. Thus this test is based on triangle presence,
	 * not on whether the owning body has a Planet component. All edge prisms are batched into
	 * one reusable mesh and drawn after world geometry, so the depth buffer occludes hidden
	 * edges. Empty chunks produce no lines.
	 */
	void drawChunkBoundaries(World& world);
}
