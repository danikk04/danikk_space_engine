#include <danikk_space_engine/tool.h>
#include <danikk_space_engine/world/world.h>

namespace danikk_space_engine
{
	namespace
	{
		constexpr ToolActionId terrain_actions[] = {ToolActionId::extrude, ToolActionId::erase};
		constexpr ToolActionId body_actions[] = {ToolActionId::extrude, ToolActionId::erase, ToolActionId::drag};
		constexpr ToolActionId place_actions[] = {ToolActionId::machinegun, ToolActionId::create_body};
		constexpr ToolActionId human_actions[] = {ToolActionId::drag, ToolActionId::create_human};

		const ToolActionId* getActions(ToolId tool, size_t& count)
		{
			switch(tool)
			{
				case ToolId::terrain:
					count = sizeof(terrain_actions) / sizeof(terrain_actions[0]);
					return terrain_actions;
				case ToolId::body:
					count = sizeof(body_actions) / sizeof(body_actions[0]);
					return body_actions;
				case ToolId::place:
					count = sizeof(place_actions) / sizeof(place_actions[0]);
					return place_actions;
				case ToolId::human:
					count = sizeof(human_actions) / sizeof(human_actions[0]);
					return human_actions;
				case ToolId::none:
				case ToolId::count:
					count = 0;
					return nullptr;
			}
			count = 0;
			return nullptr;
		}
	}

	ToolId stepTool(ToolId current, int direction)
	{
		constexpr int tool_count = static_cast<int>(ToolId::count);
		int next = static_cast<int>(current) + (direction < 0 ? -1 : 1);
		if(next < 0) next = tool_count - 1;
		if(next >= tool_count) next = 0;
		return static_cast<ToolId>(next);
	}

	ToolActionId getFirstToolAction(ToolId tool)
	{
		size_t count = 0;
		const ToolActionId* actions = getActions(tool, count);
		return count == 0 ? ToolActionId::none : actions[0];
	}

	ToolActionId stepToolAction(ToolId tool, ToolActionId current, int direction)
	{
		size_t count = 0;
		const ToolActionId* actions = getActions(tool, count);
		if(count == 0) return ToolActionId::none;

		size_t current_index = 0;
		while(current_index < count && actions[current_index] != current) current_index++;
		if(current_index == count) return actions[0];

		const int step = direction < 0 ? -1 : 1;
		int next = static_cast<int>(current_index) + step;
		if(next < 0) next = static_cast<int>(count) - 1;
		if(next >= static_cast<int>(count)) next = 0;
		return actions[next];
	}

	const char* getToolName(ToolId tool)
	{
		switch(tool)
		{
			case ToolId::none: return "None";
			case ToolId::terrain: return "Terrain";
			case ToolId::body: return "Body";
			case ToolId::place: return "Place";
			case ToolId::human: return "Human";
			case ToolId::count: break;
		}
		return "Unknown";
	}

	const char* getToolActionName(ToolActionId action)
	{
		switch(action)
		{
			case ToolActionId::none: return "None";
			case ToolActionId::extrude: return "Extrude";
			case ToolActionId::erase: return "Erase";
			case ToolActionId::drag: return "Drag";
			case ToolActionId::machinegun: return "Machinegun";
			case ToolActionId::create_body: return "Body";
			case ToolActionId::create_human: return "Create";
			case ToolActionId::count: break;
		}
		return "Unknown";
	}

	ToolApplyResult applyTool(
		ToolId tool,
		ToolActionId action,
		World& world,
		const DynamicArray<FacePtr>& selected_faces,
		size_t selected_face_count,
		bool has_hovered_face,
		const FacePtr& hovered_face,
		const RaycastHit& hit)
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
				return applyPlaceTool(world, hit, action);
			case ToolId::human:
				return applyHumanTool(world, hit, action);
			case ToolId::count:
				break;
		}
		return ToolApplyResult();
	}
}
