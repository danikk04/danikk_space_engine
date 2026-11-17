#include <danikk_space_engine/tool.h>
#include <danikk_space_engine/world/world.h>

namespace danikk_space_engine
{
	namespace
	{
		constexpr ToolActionId terrain_actions[] = {ToolActionId::extrude, ToolActionId::erase};
		constexpr ToolActionId body_actions[] = {ToolActionId::extrude, ToolActionId::erase, ToolActionId::drag};
		constexpr ToolActionId place_actions[] = {
			ToolActionId::machinegun, ToolActionId::create_body, ToolActionId::create_slope};
		constexpr ToolActionId human_actions[] = {ToolActionId::drag, ToolActionId::create_human};

		/**
		 * @brief Selects the static action list associated with one category.
		 * @param tool Tool category to inspect.
		 * @param count Receives the number of actions in the returned array.
		 * @return Pointer to static action IDs, or nullptr with count zero for None/invalid categories.
		 */
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

	/**
	 * @brief Advances or reverses the top-level tool category, wrapping at both ends.
	 * @param current Current category ordinal.
	 * @param direction Negative reverses; zero and positive values advance.
	 * @return The wrapped category ID.
	 */
	ToolId stepTool(ToolId current, int direction)
	{
		constexpr int tool_count = static_cast<int>(ToolId::count);
		int next = static_cast<int>(current) + (direction < 0 ? -1 : 1);
		if(next < 0) next = tool_count - 1;
		if(next >= tool_count) next = 0;
		return static_cast<ToolId>(next);
	}

	/**
	 * @brief Returns the first entry in a category's action table.
	 * @param tool Category to query.
	 * @return First supported action, or none when the category has no registered actions.
	 */
	ToolActionId getFirstToolAction(ToolId tool)
	{
		size_t count = 0;
		const ToolActionId* actions = getActions(tool, count);
		return count == 0 ? ToolActionId::none : actions[0];
	}

	/**
	 * @brief Steps through the action table for one category with wraparound.
	 * @param tool Category whose action list is used.
	 * @param current Current action; unsupported values fall back to the first action.
	 * @param direction Negative reverses; zero and positive values advance.
	 * @return New action, or none if the category has no actions.
	 */
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

	/**
	 * @brief Maps a top-level category ID to its UI label.
	 * @param tool Category ID.
	 * @return Static display string, or "Unknown" for an unsupported value.
	 */
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

	/**
	 * @brief Maps an action ID to its UI label.
	 * @param action Action ID.
	 * @return Static display string, or "Unknown" for an unsupported value.
	 */
	const char* getToolActionName(ToolActionId action)
	{
		switch(action)
		{
			case ToolActionId::none: return "None";
			case ToolActionId::extrude: return "Extrude";
			case ToolActionId::erase: return "Erase";
			case ToolActionId::drag: return "Drag";
			case ToolActionId::machinegun: return "Machinegun";
			case ToolActionId::create_body: return "Body: Cube";
			case ToolActionId::create_slope: return "Body: Slope";
			case ToolActionId::create_human: return "Create";
			case ToolActionId::count: break;
		}
		return "Unknown";
	}

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
