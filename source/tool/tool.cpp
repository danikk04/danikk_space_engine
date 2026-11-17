#include <danikk_space_engine/tool.h>
#include <danikk_space_engine/world/world.h>

namespace danikk_space_engine
{
	/**
	 * @brief Routes the action to the selected tool-family implementation.
	 * @param tool Active top-level category.
	 * @param action Selected action ID.
	 * @param world World the handler may modify.
	 * @param hit Current camera raycast result.
	 * @param build_part_size Per-axis size for BuildPart placement actions.
	 * @return Handler result for optional selection replacement; unsupported category IDs return an empty result.
	 */
	ToolApplyResult applyTool(
		ToolId tool,
		ToolActionId action,
		World& world,
		const RaycastHit& hit,
		const vec3& build_part_size)
	{
		switch(tool)
		{
			case ToolId::none:
				return ToolApplyResult();
			case ToolId::terrain:
				return applyTerrainTool(world, action, hit);
			case ToolId::place:
				return applyPlaceTool(world, hit, action, build_part_size);
			case ToolId::human:
				return applyHumanTool(world, hit, action);
			case ToolId::event:
				return applyEventTool(world, hit, action);
			case ToolId::drag:
				return ToolApplyResult();
			case ToolId::count:
				break;
		}
		return ToolApplyResult();
	}
}
