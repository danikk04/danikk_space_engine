#include <danikk_space_engine/tool_catalog.h>

namespace danikk_space_engine
{
	namespace
	{
		const ToolCategoryDefinition categories[]
		{
			{0, "Terrain"},
			{1, "Body"},
			{2, "Place"},
			{3, "Human"}
		};

		const ToolCatalogEntry entries[]
		{
			{no_tool_catalog_entry, 0, "None", "", ToolId::none, ToolActionId::none},
			{1, 0, "Extrude terrain", "T-EXT", ToolId::terrain, ToolActionId::extrude},
			{2, 0, "Erase terrain", "T-ERS", ToolId::terrain, ToolActionId::erase},
			{3, 1, "Extrude body", "B-EXT", ToolId::body, ToolActionId::extrude},
			{4, 1, "Erase body", "B-ERS", ToolId::body, ToolActionId::erase},
			{5, 1, "Drag body", "B-DRG", ToolId::body, ToolActionId::drag},
			{6, 2, "Cube", "CUBE", ToolId::place, ToolActionId::create_body},
			{7, 2, "Slope", "SLOPE", ToolId::place, ToolActionId::create_slope},
			{8, 2, "MG-42", "MG42", ToolId::place, ToolActionId::machinegun},
			{9, 3, "Drag human", "H-DRG", ToolId::human, ToolActionId::drag},
			{10, 3, "Create human", "HUMAN", ToolId::human, ToolActionId::create_human}
		};
	}

	/** @copydoc getToolCatalogEntry */
	const ToolCatalogEntry& getToolCatalogEntry(ToolCatalogEntryId id)
	{
		if(id >= sizeof(entries) / sizeof(entries[0]))
		{
			danikk_engine::formatFatalError("Invalid tool catalog entry ID %", id);
		}
		return entries[id];
	}

	/** @copydoc getToolCategoryCount */
	uint32 getToolCategoryCount()
	{
		return sizeof(categories) / sizeof(categories[0]);
	}

	/** @copydoc getToolCategory */
	const ToolCategoryDefinition& getToolCategory(uint32 index)
	{
		if(index >= getToolCategoryCount())
		{
			danikk_engine::formatFatalError("Invalid tool category index %", index);
		}
		return categories[index];
	}

	/** @copydoc getToolCatalogEntryTotalCount */
	uint32 getToolCatalogEntryTotalCount()
	{
		return sizeof(entries) / sizeof(entries[0]) - 1;
	}

	/** @copydoc getToolCatalogEntryCount */
	uint32 getToolCatalogEntryCount(ToolCategoryId category_id)
	{
		uint32 count = 0;
		for(uint32 id = 1; id < sizeof(entries) / sizeof(entries[0]); ++id)
		{
			if(entries[id].category_id == category_id) count++;
		}
		return count;
	}

	/** @copydoc getToolCatalogEntryId */
	ToolCatalogEntryId getToolCatalogEntryId(ToolCategoryId category_id, uint32 index)
	{
		for(uint32 id = 1; id < sizeof(entries) / sizeof(entries[0]); ++id)
		{
			if(entries[id].category_id != category_id) continue;
			if(index == 0) return id;
			index--;
		}
		danikk_engine::formatFatalError(
			"Tool catalog index % is out of range for category %", index, category_id);
		return no_tool_catalog_entry;
	}
}
