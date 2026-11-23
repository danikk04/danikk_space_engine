#include <danikk_space_engine/tool.h>
#include <danikk_space_engine/tool_catalog.h>
#include <danikk_space_engine/world.h>

#include <danikk_space_engine/systems/raycast/raycast.h>

namespace danikk_space_engine
{
	/**
	 * @brief Invokes the callback registered at the selected catalog index.
	 * @param tool_id Index of the selected catalog entry; zero is a no-op.
	 * @param world World the handler may modify.
	 * @param hit Current camera raycast result.
	 * @param build_part_size Per-axis size for BuildPart placement actions.
	 * @param build_part_rotation_step Snapped quarter-turn angle for BuildPart placement.
	 * @return Handler result for optional selection replacement; unsupported category IDs return an empty result.
	 */
	ToolApplyResult applyTool(
		ToolCatalogEntryId tool_id,
		World& world,
		const RaycastHit& hit,
		const vec3& build_part_size,
		uint8 build_part_rotation_step)
	{
		const ToolCatalogEntry& entry = getToolCatalogEntry(tool_id);
		if(entry.handler != nullptr)
			return entry.handler(world, hit, build_part_size, build_part_rotation_step);
		return ToolApplyResult();
	}
}
