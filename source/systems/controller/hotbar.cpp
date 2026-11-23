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
		active_tool_id = hotbar_entry_ids[slot_index];
	}

	/** @copydoc ControllerSystem::selectToolCatalogEntry */
	void ControllerSystem::selectToolCatalogEntry(ToolCatalogEntryId entry_id)
	{
		getToolCatalogEntry(entry_id);
		hotbar_entry_ids[selected_hotbar_slot] = entry_id;
		active_tool_id = entry_id;
	}

	/** @copydoc ControllerSystem::getActiveTool */
	const ToolCatalogEntry& ControllerSystem::getActiveTool() const
	{
		return getToolCatalogEntry(active_tool_id);
	}
}
