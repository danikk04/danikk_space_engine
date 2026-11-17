#include <danikk_space_engine/tool.h>
#include <danikk_space_engine/world/world.h>

namespace danikk_space_engine
{
	/**
	 * @brief Routes the action to the selected tool-family implementation.
	 * @param tool Active top-level category.
	 * @param action Selected action ID.
	 * @param world World the handler may modify.
	 * @param selected_faces Current face list, selected prefix followed by optional hover face.
	 * @param selected_face_count Number of persistent selection entries.
	 * @param has_hovered_face Whether hovered_face is valid.
	 * @param hovered_face Current cursor target.
	 * @param hit Current camera raycast result.
	 * @param build_part_size Per-axis size for BuildPart placement actions.
	 * @return Handler result for optional selection replacement; unsupported category IDs return an empty result.
	 */
	ToolApplyResult applyTool(
		ToolId tool,
		ToolActionId action,
		World& world,
		const DynamicArray<FacePtr>& selected_faces,
		size_t selected_face_count,
		bool has_hovered_face,
		const FacePtr& hovered_face,
		const RaycastHit& hit,
		const vec3& build_part_size)
	{
		switch(tool)
		{
			case ToolId::none:
				return ToolApplyResult();
			case ToolId::terrain:
				return applyTerrainTool(world, action, hit);
			case ToolId::body:
				return applyBodyTool(
					world, action, selected_faces, selected_face_count, has_hovered_face, hovered_face);
			case ToolId::place:
				return applyPlaceTool(world, hit, action, build_part_size);
			case ToolId::human:
				return applyHumanTool(world, hit, action);
			case ToolId::count:
				break;
		}
		return ToolApplyResult();
	}
}
