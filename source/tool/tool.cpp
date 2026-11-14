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

	CreateObjectId stepCreateObject(CreateObjectId current, int direction)
	{
		constexpr int object_count = static_cast<int>(CreateObjectId::count);
		int next = static_cast<int>(current) + (direction < 0 ? -1 : 1);
		if(next < 0) next = object_count - 1;
		if(next >= object_count) next = 0;
		return static_cast<CreateObjectId>(next);
	}

	const char* getToolName(ToolId tool)
	{
		switch(tool)
		{
			case ToolId::none: return "None";
			case ToolId::select: return "Select";
			case ToolId::expand: return "Expand";
			case ToolId::erase_faces: return "Erase";
			case ToolId::create: return "Create";
			case ToolId::count: break;
		}
		return "Unknown";
	}

	const char* getCreateObjectName(CreateObjectId object)
	{
		switch(object)
		{
			case CreateObjectId::none: return "None";
			case CreateObjectId::machinegun: return "Machinegun";
			case CreateObjectId::volume_cell: return "VolumeCell";
			case CreateObjectId::count: break;
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
		CreateObjectId create_object)
	{
		switch(tool)
		{
			case ToolId::none: return applyNoneTool(world, selected_faces, false);
			case ToolId::select:
				return applySelectTool(selected_faces, selected_face_count, has_hovered_face, hovered_face);
			case ToolId::expand:
			case ToolId::erase_faces:
			{
				if(selected_face_count > 0)
				{
					return tool == ToolId::expand
						? applyExpandTool(world, selected_faces, true)
						: applyEraseTool(world, selected_faces, true);
				}
				if(!has_hovered_face) return ToolApplyResult();
				DynamicArray<FacePtr> hovered_faces;
				hovered_faces.push(hovered_face);
				return tool == ToolId::expand
					? applyExpandTool(world, hovered_faces, false)
					: applyEraseTool(world, hovered_faces, false);
			}
			case ToolId::create:
				return applyCreateTool(world, hit, create_object);
			case ToolId::count: break;
		}
		return ToolApplyResult();
	}
}
