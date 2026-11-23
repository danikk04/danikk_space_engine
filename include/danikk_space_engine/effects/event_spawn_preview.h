#pragma once

#include <danikk_space_engine/fwd.h>
#include <danikk_space_engine/tool.h>

namespace danikk_space_engine
{
	/** @brief Draws translucent human figures at the selected event's resolved spawn positions. */
	void drawEventSpawnPreview(World& world, const RaycastHit& hit, FactionId faction_id);
}
