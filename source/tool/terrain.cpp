#include <danikk_space_engine/tool.h>
#include <danikk_space_engine/world.h>
#include <danikk_space_engine/material.h>
#include <danikk_space_engine/systems/planet/system.h>

namespace danikk_space_engine
{
	/**
	 * @brief Applies a six-metre-radius stone fill or terrain cut to a planet hit.
	 * @param world World containing the target planet.
	 * @param action Terrain action; only extrude and erase are recognized.
	 * @param hit Raycast result defining the triangle-centered edit.
	 * @return Empty selection result; misses, non-planet bodies, and unsupported actions do nothing.
	 */
	ToolApplyResult applyTerrainTool(World& world, ToolActionId action, const RaycastHit& hit)
	{
		if(!hit.hit || hit.hit_build_part || !world.planets.isPlanet(hit.body_id)) return ToolApplyResult();
		switch(action)
		{
			case ToolActionId::extrude:
				world.planets.editPlanetGround(
					hit.body_id, hit.position, 6.0, default_materials::stone);
				break;
			case ToolActionId::erase:
				world.planets.editPlanetGround(
					hit.body_id, hit.position, 6.0, default_materials::void_material);
				break;
			default:
				break;
		}
		return ToolApplyResult();
	}
}
