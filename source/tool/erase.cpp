#include <danikk_space_engine/tool.h>
#include <danikk_space_engine/world/world.h>

namespace danikk_space_engine
{
	ToolApplyResult applyEraseTool(
		World& world,
		const DynamicArray<FacePtr>& faces,
		bool using_selection)
	{
		(void)using_selection;
		world.deleteFaces(faces);
		return ToolApplyResult();
	}
}
