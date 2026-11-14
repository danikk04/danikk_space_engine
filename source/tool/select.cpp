#include <danikk_space_engine/tool.h>

namespace danikk_space_engine
{
	ToolApplyResult applySelectTool(
		const DynamicArray<FacePtr>& selected_faces,
		size_t selected_face_count,
		bool has_hovered_face,
		const FacePtr& hovered_face)
	{
		ToolApplyResult result;
		result.replace_selected_faces = true;
		if(!has_hovered_face) return result;

		bool was_selected = false;
		for(size_t index = 0; index < selected_face_count; index++)
		{
			if(selected_faces[index] == hovered_face)
			{
				was_selected = true;
				continue;
			}
			result.selected_faces.push(selected_faces[index]);
		}
		if(!was_selected) result.selected_faces.push(hovered_face);
		return result;
	}
}
