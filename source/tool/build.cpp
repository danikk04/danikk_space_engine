#include <danikk_space_engine/tool.h>
#include <danikk_space_engine/world/world.h>

namespace danikk_space_engine
{
	ToolApplyResult applyBuildTool(
		World& world,
		const DynamicArray<FacePtr>& faces,
		size_t selected_face_count,
		bool has_hovered_face,
		const FacePtr& hovered_face,
		const RaycastHit& hit,
		BuildActionId action)
	{
		if(action == BuildActionId::extrude_face)
		{
			if(selected_face_count > 0)
			{
				DynamicArray<FacePtr> selected_targets;
				selected_targets.reserve(selected_face_count);
				for(size_t index = 0; index < selected_face_count; index++)
				{
					selected_targets.push(faces[index]);
				}
				return applyExtrudeTool(world, selected_targets, true);
			}
			if(!has_hovered_face) return ToolApplyResult();
			DynamicArray<FacePtr> hovered_target;
			hovered_target.push(hovered_face);
			return applyExtrudeTool(world, hovered_target, false);
		}

		return applyBuildPlacementTool(world, hit, action);
	}
}
