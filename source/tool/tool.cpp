#include <danikk_space_engine/tool.h>

namespace danikk_space_engine
{
	ToolId stepTool(ToolId current, int direction)
	{
		constexpr int tool_count = static_cast<int>(ToolId::count);
		int next = static_cast<int>(current) + (direction < 0 ? -1 : 1);
		if(next < 0) next = tool_count - 1;
		if(next >= tool_count) next = 0;
		return static_cast<ToolId>(next);
	}

	BuildActionId stepBuildAction(BuildActionId current, int direction)
	{
		constexpr int action_count = static_cast<int>(BuildActionId::count);
		int next = static_cast<int>(current) + (direction < 0 ? -1 : 1);
		if(next < 0) next = action_count - 1;
		if(next >= action_count) next = 0;
		return static_cast<BuildActionId>(next);
	}

	const char* getToolName(ToolId tool)
	{
		switch(tool)
		{
			case ToolId::none: return "None";
			case ToolId::select: return "Select";
			case ToolId::drag: return "Drag";
			case ToolId::build: return "Build";
			case ToolId::erase_faces: return "Erase";
			case ToolId::count: break;
		}
		return "Unknown";
	}

	const char* getBuildActionName(BuildActionId action)
	{
		switch(action)
		{
			case BuildActionId::none: return "None";
			case BuildActionId::place_machinegun: return "Place Machinegun";
			case BuildActionId::create_world_body: return "Create WorldBody";
			case BuildActionId::extrude_terrain: return "Extrude Terrain";
			case BuildActionId::count: break;
		}
		return "Unknown";
	}

	ToolApplyResult applyTool(
		ToolId tool,
		World& world,
		const DynamicArray<FacePtr>& selected_faces,
		size_t selected_face_count,
		bool has_hovered_face,
		const FacePtr& hovered_face,
		const RaycastHit& hit,
		BuildActionId build_action)
	{
		switch(tool)
		{
			case ToolId::none: return applyNoneTool(world, selected_faces, false);
			case ToolId::select:
				return applySelectTool(selected_faces, selected_face_count, has_hovered_face, hovered_face);
			case ToolId::drag:
				return ToolApplyResult();
			case ToolId::erase_faces:
			{
				if(selected_face_count > 0)
				{
					return applyEraseTool(world, selected_faces, true);
				}
				if(!has_hovered_face) return ToolApplyResult();
				DynamicArray<FacePtr> hovered_faces;
				hovered_faces.push(hovered_face);
				return applyEraseTool(world, hovered_faces, false);
			}
			case ToolId::build:
				return applyBuildTool(world, hit, build_action);
			case ToolId::count: break;
		}
		return ToolApplyResult();
	}
}
