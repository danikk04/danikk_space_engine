#include <danikk_space_engine/systems/controller/controller.h>
#include <danikk_space_engine/tool_catalog.h>

#include <danikk_engine/danikk_engine.h>

namespace danikk_space_engine
{
	/** @copydoc ControllerSystem::selectHotbarSlot */
	void ControllerSystem::selectHotbarSlot(uint32 slot_index)
	{
		if(slot_index >= 36)
			danikk_engine::formatFatalError("Hotbar slot index % is out of range", slot_index);
		selected_hotbar_slot = slot_index;
		const ToolCatalogEntry& entry = getToolCatalogEntry(hotbar_entry_ids[slot_index]);
		active_tool = entry.tool;
		selected_tool_action = entry.action;
	}

	/** @copydoc ControllerSystem::selectToolCatalogEntry */
	void ControllerSystem::selectToolCatalogEntry(ToolCatalogEntryId entry_id)
	{
		const ToolCatalogEntry& entry = getToolCatalogEntry(entry_id);
		hotbar_entry_ids[selected_hotbar_slot] = entry_id;
		active_tool = entry.tool;
		selected_tool_action = entry.action;
	}
}
