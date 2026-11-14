#include <danikk_space_engine/tool.h>
#include <danikk_space_engine/world/world.h>

namespace danikk_space_engine
{
	ToolApplyResult applyExpandTool(
		World& world,
		const DynamicArray<FacePtr>& faces,
		bool using_selection)
	{
		ToolApplyResult result;
		DynamicArray<FacePtr> extruded_faces = world.extrudeFaces(faces, 1.0f);
		if(using_selection && extruded_faces.size() > 0)
		{
			result.replace_selected_faces = true;
			result.selected_faces = static_cast<DynamicArray<FacePtr>&&>(extruded_faces);
		}
		return result;
	}
}
