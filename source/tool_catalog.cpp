#include <danikk_space_engine/tool_catalog.h>

namespace danikk_space_engine
{
	namespace
	{
		constexpr ToolCategoryId all_category_id = 0;
		constexpr ToolCategoryId tools_category_id = 1;
		constexpr ToolCategoryId parts_category_id = 2;
		constexpr ToolCategoryId human_category_id = 3;
		constexpr ToolCategoryId event_category_id = 4;

		const ToolCategoryDefinition categories[]
		{
			{"All"},
			{"Tools"},
			{"Parts"},
			{"Human"},
			{"Event"}
		};

		const ToolCatalogEntry entries[]
		{
			{all_category_id, "None", "", ToolId::none, ToolActionId::none},
			{tools_category_id, "Extrude terrain", "T-EXT", ToolId::terrain, ToolActionId::extrude},
			{tools_category_id, "Erase terrain", "T-ERS", ToolId::terrain, ToolActionId::erase},
			{tools_category_id, "Drag", "DRAG", ToolId::drag, ToolActionId::drag},
			{parts_category_id, "Cube", "CUBE", ToolId::place, ToolActionId::create_body},
			{parts_category_id, "Slope", "SLOPE", ToolId::place, ToolActionId::create_slope},
			{parts_category_id, "MG-42", "MG42", ToolId::place, ToolActionId::machinegun},
			{human_category_id, "Create human", "HUMAN", ToolId::human, ToolActionId::create_human},
			{event_category_id, "Spawn 5 allies", "5ALL", ToolId::event, ToolActionId::spawn_5_allies},
			{event_category_id, "Spawn 5 pirates", "5PIR", ToolId::event, ToolActionId::spawn_5_pirates}
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
		if(category_id == all_category_id) return getToolCatalogEntryTotalCount();
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
		if(category_id == all_category_id)
		{
			if(index < getToolCatalogEntryTotalCount()) return index + 1;
			danikk_engine::formatFatalError("Tool catalog index % is out of range for category %", index, category_id);
		}
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
