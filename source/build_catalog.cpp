#include <danikk_space_engine/build_catalog.h>

namespace danikk_space_engine
{
	namespace
	{
		const BuildDefinition definitions[build_definition_ids::count]
		{
			{build_definition_ids::none, BuildCategoryId::structural, "None", "", ToolId::none, ToolActionId::none},
			{build_definition_ids::cube, BuildCategoryId::structural, "Cube", "CUBE", ToolId::place, ToolActionId::create_body},
			{build_definition_ids::slope, BuildCategoryId::structural, "Slope", "SLOPE", ToolId::place, ToolActionId::create_slope},
			{build_definition_ids::machinegun, BuildCategoryId::weapons, "MG-42", "MG42", ToolId::place, ToolActionId::machinegun}
		};
	}

	/** @copydoc getBuildDefinition */
	const BuildDefinition& getBuildDefinition(BuildDefinitionId id)
	{
		if(id >= build_definition_ids::count)
		{
			danikk_engine::formatFatalError("Invalid build definition ID %", id);
		}
		return definitions[id];
	}

	/** @copydoc getBuildDefinitionCount */
	uint32 getBuildDefinitionCount(BuildCategoryId category)
	{
		uint32 count = 0;
		for(uint32 id = 1; id < build_definition_ids::count; ++id)
		{
			if(definitions[id].category == category) count++;
		}
		return count;
	}

	/** @copydoc getBuildDefinitionId */
	BuildDefinitionId getBuildDefinitionId(BuildCategoryId category, uint32 index)
	{
		for(uint32 id = 1; id < build_definition_ids::count; ++id)
		{
			if(definitions[id].category != category) continue;
			if(index == 0) return id;
			index--;
		}
		danikk_engine::formatFatalError("Build catalog index % is out of range for category %",
			index, static_cast<uint32>(category));
		return build_definition_ids::none;
	}

	/** @copydoc getBuildCategoryName */
	const char* getBuildCategoryName(BuildCategoryId category)
	{
		switch(category)
		{
			case BuildCategoryId::structural: return "Structural";
			case BuildCategoryId::weapons: return "Weapons";
			case BuildCategoryId::count: break;
		}
		return "Unknown";
	}
}
