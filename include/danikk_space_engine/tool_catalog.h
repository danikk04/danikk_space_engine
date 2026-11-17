#pragma once

#include <danikk_space_engine/id.h>
#include <danikk_space_engine/tool.h>

namespace danikk_space_engine
{
	/** @brief Reserved entry ID used by empty hotbar slots. */
	inline constexpr ToolCatalogEntryId no_tool_catalog_entry = 0;

	/** @brief Display record for one top-level tool-menu category. */
	struct ToolCategoryDefinition
	{
		/** @brief Label shown in the tool menu. */
		const char* name = "";
	};

	/** @brief Selectable command shown in the tool menu and assignable to a hotbar slot. */
	struct ToolCatalogEntry
	{
		/** @brief Menu category that contains this command. */
		ToolCategoryId category_id = 0;
		/** @brief Full command name. */
		const char* name = "";
		/** @brief Short label shown in hotbar and menu cells. */
		const char* short_name = "";
		/** @brief Existing tool dispatcher category used to execute the command. */
		ToolId tool = ToolId::none;
		/** @brief Existing tool dispatcher action executed by the command. */
		ToolActionId action = ToolActionId::none;
	};

	/**
	 * @brief Returns an immutable command record by catalog ID.
	 * @param id Stable tool-catalog entry ID; zero represents an empty slot.
	 * @return Command record; an invalid ID triggers a fatal diagnostic.
	 */
	const ToolCatalogEntry& getToolCatalogEntry(ToolCatalogEntryId id);

	/** @brief Returns the number of registered tool-menu categories. */
	uint32 getToolCategoryCount();

	/**
	 * @brief Returns a tool-menu category by its table index.
	 * @param index Zero-based category-table index.
	 * @return Immutable category record; an invalid index triggers a fatal diagnostic.
	 */
	const ToolCategoryDefinition& getToolCategory(uint32 index);

	/** @brief Returns the number of registered commands, excluding the empty-slot entry. */
	uint32 getToolCatalogEntryTotalCount();

	/**
	 * @brief Counts commands displayed in one category.
	 * @param category_id Category being enumerated.
	 * @return Number of commands assigned to the category.
	 */
	uint32 getToolCatalogEntryCount(ToolCategoryId category_id);

	/**
	 * @brief Returns a command ID from a category's display order.
	 * @param category_id Category being enumerated.
	 * @param index Zero-based position within the category.
	 * @return Command ID; an invalid index triggers a fatal diagnostic.
	 */
	ToolCatalogEntryId getToolCatalogEntryId(ToolCategoryId category_id, uint32 index);
}
