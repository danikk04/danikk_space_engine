#include <danikk_space_engine/tool.h>
#include <danikk_space_engine/world/world.h>

namespace danikk_space_engine
{
	ToolApplyResult applyBuildTool(
		World& world,
		const RaycastHit& hit,
		BuildActionId action)
	{
		return applyBuildPlacementTool(world, hit, action);
	}
}
