#include <danikk_space_engine/tool.h>
#include <danikk_space_engine/world.h>
#include <danikk_space_engine/material.h>
#include <danikk_space_engine/systems/planet/system.h>
#include <danikk_framework/log.h>

#include <danikk_space_engine/systems/raycast/raycast.h>

namespace danikk_space_engine
{
	namespace
	{
		constexpr bool enable_terrain_edit_tool_logs = true;
	}

	/**
	 * @brief Applies a six-metre-radius stone fill or terrain cut to a planet hit.
	 * @param world World containing the target planet.
	 * @param action Terrain action; only extrude and erase are recognized.
	 * @param hit Raycast result defining the triangle-centered edit.
	 * @return Empty selection result; misses, non-planet bodies, and unsupported actions do nothing.
	 */
	static ToolApplyResult applyTerrainTool(World& world, const RaycastHit& hit,
		MaterialId material_id, const char* action_name)
	{
		if(!hit.hit || hit.hit_build_part)
		{
			if constexpr(enable_terrain_edit_tool_logs)
				danikk_framework::formatLogInfo(
				"Terrain tool ignored hit: hit=% terrain=% build_part=% action=%",
				hit.hit, hit.hit_terrain, hit.hit_build_part, action_name);
			return ToolApplyResult();
		}
		const PlanetId planet_id = hit.planet_id;
		if(!world.getPlanetSystem().isPlanet(planet_id))
		{
			if constexpr(enable_terrain_edit_tool_logs)
				danikk_framework::formatLogWarning(
				"Terrain tool hit has no valid planet: action=% planet=% body=% terrain=%",
				action_name, planet_id, hit.body_id, hit.hit_terrain);
			return ToolApplyResult();
		}
		const bool applied = world.getPlanetSystem().editPlanetGround(
			planet_id, hit.position, 6.0, material_id);
		if constexpr(enable_terrain_edit_tool_logs)
			danikk_framework::formatLogInfo(
				"Terrain edit result: action=% applied=% planet=% position=(%,%,%)",
				action_name, applied, planet_id,
				hit.position.x, hit.position.y, hit.position.z);
		return ToolApplyResult();
	}

	ToolApplyResult applyTerrainExtrudeTool(World& world, const RaycastHit& hit,
		const vec3&, uint8)
	{
		return applyTerrainTool(world, hit, default_materials::stone, "extrude");
	}

	ToolApplyResult applyTerrainEraseTool(World& world, const RaycastHit& hit,
		const vec3&, uint8)
	{
		return applyTerrainTool(world, hit, default_materials::void_material, "erase");
	}
}
