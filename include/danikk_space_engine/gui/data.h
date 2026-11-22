#pragma once

#include <danikk_engine/gui.h>
#include <danikk_engine/style.h>
#include <danikk_space_engine/default.h>
#include <danikk_space_engine/tool_catalog.h>

namespace danikk_space_engine::gui_detail
{
	/** @brief Stable source and callback used to produce a label's current text. */
	struct TextSource
	{
		const void* source = nullptr;
		GUIElement::TextWriter writer = nullptr;
	};

	/** @brief On-screen player message list and its text presentation. */
	struct MessageLog
	{
		/** @brief One message and its remaining display lifetime. */
		struct Message
		{
			String text;
			float remaining_seconds = 10.0f;
		};

		/** @brief Messages shown newest-first in the screen's top-right corner. */
		DynamicArray<Message> messages;
		/** @brief Multiline widget displaying the active message list. */
		GUIElement label;
		/** @brief Shared bitmap text style for player-facing messages. */
		TextStyle text_style;
	};

	/** @brief Debug overlay label, data binding, and memory sampling state. */
	struct DebugOverlay
	{
		GUIElement label;
		TextStyle text_style;
		TextSource source;
		size_t memory_sample_bytes = 0;
		float memory_sample_timer = 1.0f;
	};

	/** @brief Hotbar elements, styles, and live controller bindings. */
	struct Hotbar
	{
		static constexpr uint32 column_count = 12;
		static constexpr uint32 row_count = 3;
		/** @brief Slot backgrounds indexed in row-major order. */
		GUIElement slots[column_count * row_count];
		/** @brief Keyboard shortcut labels layered over the slot backgrounds. */
		GUIElement slot_labels[column_count * row_count];
		/** @brief Bound tool names layered inside the slots. */
		GUIElement item_labels[column_count * row_count];
		/** @brief Background fill shared by all slots. */
		DrawStyle slot_fill_style;
		/** @brief Default outline for unselected slots. */
		BorderDrawStyle slot_border_style;
		/** @brief Highlight outline for the selected slot. */
		BorderDrawStyle selected_slot_border_style;
		/** @brief Font style for shortcut labels. */
		TextStyle slot_text_style;
		/** @brief Font style for assigned tool names. */
		TextStyle item_text_style;
		/** @brief Stable controller-owned slot assignments. */
		const ToolCatalogEntryId* entry_ids_source = nullptr;
		/** @brief Controller-owned active slot. */
		const uint32* selected_slot_source = nullptr;
		/** @brief Controller-owned catalog visibility state. */
		const bool* catalog_open_source = nullptr;
	};

	/** @brief Selected-unit summary panel and its visibility binding. */
	struct SelectedUnitsPanel
	{
		GUIElement panel;
		GUIElement label;
		TextStyle text_style;
		bool (*has_units)(const void*) = nullptr;
		const void* visibility_source = nullptr;
	};

	/** @brief Tool catalog window, category tabs, item cells, and preview camera binding. */
	struct ToolCatalogWindow
	{
		static constexpr uint32 category_capacity = 16;
		static constexpr uint32 item_capacity = 64;
		/** @brief Full-screen modal background for the catalog. */
		GUIElement panel;
		/** @brief Catalog close button and its text child. */
		GUIElement close_button;
		GUIElement close_label;
		/** @brief Category selector widgets. */
		Array<GUIElement, category_capacity> category_buttons;
		Array<GUIElement, category_capacity> category_labels;
		/** @brief Tool entry cells and their labels/previews. */
		Array<GUIElement, item_capacity> item_buttons;
		Array<GUIElement, item_capacity> item_labels;
		/** @brief Typography shared by category and close labels. */
		TextStyle text_style;
		/** @brief Category currently shown by the catalog. */
		ToolCategoryId selected_category = 0;
		/** @brief Camera input used by cell preview callbacks. */
		const void* preview_camera_source = nullptr;
		void (*read_preview_camera)(const void*, vec3&, vec3&, float&) = nullptr;
		/** @brief Cached camera view values, sampled once per GUI frame. */
		vec3 preview_camera_front = vec3(0.0f, 0.0f, -1.0f);
		vec3 preview_camera_up = vec3(0.0f, 1.0f, 0.0f);
		float preview_vertical_fov_degrees = 90.0f;
	};

	/** @brief Shared fills and borders used by multiple GUI windows. */
	struct SharedStyles
	{
		DrawStyle panel_fill;
		DrawStyle button_fill;
		DrawStyle selected_button_fill;
		BorderDrawStyle border;
	};
}

namespace danikk_space_engine
{
	/** @brief An input requested by the GUI for the application/controller to apply. */
	enum class GUIActionType : uint8
	{
		toggle_tool_catalog,
		close_tool_catalog,
		select_tool_catalog_entry,
		select_hotbar_slot
	};

	/** @brief A GUI action and its optional catalog-entry or hotbar-slot value. */
	struct GUIAction
	{
		GUIActionType type = GUIActionType::toggle_tool_catalog;
		ToolCatalogEntryId catalog_entry_id = no_tool_catalog_entry;
		uint32 hotbar_slot = 0;
	};
}
