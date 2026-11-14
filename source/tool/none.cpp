#include <danikk_space_engine/tool.h>

namespace danikk_space_engine
{
	ToolApplyResult applyNoneTool(
		World& world,
		const DynamicArray<FacePtr>& faces,
		bool using_selection)
	{
		(void)world;
		(void)faces;
		(void)using_selection;
		return ToolApplyResult();
	}
}
