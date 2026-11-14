#pragma once

namespace danikk_space_engine
{
	class World;
	struct RaycastHit;

	void drawVolumeCellPlacementPreview(World& world, const RaycastHit& hit);
}
