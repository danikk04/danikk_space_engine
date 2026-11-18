#pragma once

#include <danikk_engine/gui.h>
#include <danikk_engine/fps.h>
#include <danikk_engine/style.h>
#include <danikk_space_engine/default.h>
#include <danikk_space_engine/tool_catalog.h>

namespace danikk_space_engine
{

	/**
	 * @brief Owns the debug overlay, message list, 3x12 hotbar, and modal tool catalog.
	 *
	 * The GUI text reports FPS, process memory, BuildPart size when applicable, camera speed,
	 * and camera position. Process memory is sampled at roughly one-second
	 * intervals; a zero reading is displayed as unavailable.
	 */
	class GUI
	{
		/** @brief One player-facing message and its remaining display lifetime. */
		struct Message
		{
			String text;
			float remaining_seconds = 10.0f;
		};

		/** @brief Messages shown newest-first in the screen's top-right corner. */
		DynamicArray<Message> messages;
		/** @brief Multiline widget displaying the active message list. */
		GUIElement messages_label;
		/** @brief Shared bitmap text style for player-facing messages. */
		TextStyle messages_text_style;
		/** @brief Number of columns in the visual build hotbar. */
		static constexpr uint32 hotbar_column_count = 12;
		/** @brief Number of rows in the visual build hotbar. */
		static constexpr uint32 hotbar_row_count = 3;
		/** @brief Capacity for category-tab widgets; catalog data remains table-driven. */
		static constexpr uint32 catalog_category_capacity = 16;
		/** @brief Capacity for detail-cell widgets; catalog data remains table-driven. */
		static constexpr uint32 catalog_item_capacity = 64;
		/** @brief Empty toolbar slot elements, laid out row by row from the bottom. */
		GUIElement hotbar_slots[hotbar_column_count * hotbar_row_count];
		/** @brief Small keyboard shortcut labels drawn above each slot's background. */
		GUIElement hotbar_slot_labels[hotbar_column_count * hotbar_row_count];
		/** @brief Compact item names displayed inside assigned hotbar slots. */
		GUIElement hotbar_item_labels[hotbar_column_count * hotbar_row_count];
		/** @brief Shared dark-gray fill style for every empty toolbar slot. */
		DrawStyle hotbar_slot_fill_style;
		/** @brief Shared thin gray outline style for every toolbar slot. */
		BorderDrawStyle hotbar_slot_border_style;
		/** @brief Brighter gray outline style for the currently selected toolbar slot. */
		BorderDrawStyle hotbar_selected_slot_border_style;
		/** @brief Shared small-font style for toolbar keyboard shortcut labels. */
		TextStyle hotbar_slot_text_style;
		/** @brief Shared small-font style for assigned hotbar item names. */
		TextStyle hotbar_item_text_style;
		/** @brief Modal tool catalog opened by the G key. */
		GUIElement catalog_panel;
		/** @brief Button that closes the catalog window. */
		GUIElement catalog_close_button;
		/** @brief Text shown inside the catalog close button. */
		GUIElement catalog_close_label;
		/** @brief Category tab backgrounds, sized from the category definition table. */
		Array<GUIElement, catalog_category_capacity> catalog_category_buttons;
		/** @brief Category tab text, sized from the category definition table. */
		Array<GUIElement, catalog_category_capacity> catalog_category_labels;
		/** @brief Detail entry buttons, sized from the tool catalog table. */
		Array<GUIElement, catalog_item_capacity> catalog_item_buttons;
		/** @brief Compact names displayed inside catalog command cells. */
		Array<GUIElement, catalog_item_capacity> catalog_item_labels;
		/** @brief Shared text style for catalog labels. */
		TextStyle catalog_text_style;
		/** @brief Fill and outline styles used by the catalog window and buttons. */
		DrawStyle catalog_panel_style;
		DrawStyle catalog_button_style;
		DrawStyle catalog_selected_button_style;
		BorderDrawStyle catalog_border_style;
		/** @brief Tool category currently shown in the menu. */
		ToolCategoryId selected_tool_category = 0;
		/** @brief Engine GUI element used to display the game's multiline debug readout. */
		GUIElement debug_info_label;
		/** @brief Most recently sampled process memory usage in bytes; zero means unavailable. */
		size_t memory_sample_bytes = 0;
		/** @brief Elapsed frame time since the previous process-memory sample. */
		float memory_sample_timer = 1.0f;

		/** @brief Refreshes visible commands and category highlighting in the tool menu. */
		void refreshToolCatalog();
		/** @brief Ages messages, removes expired entries, and refreshes the visible list. */
		void updateMessages(float dt);
	public:

		/**
		 * @brief Configures the debug label and positions the empty three-row tool hotbar.
		 *
		 * The debug label uses the shared bitmap font and 0.014 screen-relative character dimensions.
		 * The hotbar contains twelve columns and three rows of gray slots, centered near the bottom
		 * edge. Its bottom row shows number symbols, middle row shows Shift shortcuts, and top row
		 * shows Ctrl shortcuts. The tool catalog uses All, Tools, Parts, and Human categories.
		 */
		void define();

		/**
		 * @brief Adds the debug overlay, hotbar, and tool catalog hierarchy to the engine GUI root.
		 * Call after define() so parent/child relationships and callbacks are registered.
		 */
		void compose();

		/**
		 * @brief Refreshes debug text, hotbar selection, catalog visibility, and sampled memory.
		 *
		 * Samples process memory when the accumulated target-frame delay reaches one second, then
		 * builds one multiline string containing FPS, RAM in MiB, relevant BuildPart controls,
		 * camera speed in m/s, and camera position truncated to hundredths. A zero memory sample is
		 * shown as unavailable. Pressing G toggles the modal tool catalog; selected slot borders are
		 * updated from controller state. Rendering is driven by the surrounding engine frame loop.
		 */
		void frame();

		/**
		 * @brief Adds a player-facing message at the top of the on-screen list.
		 * @param text Message text; the message remains visible for ten seconds.
		 */
		void addMessage(const String& text);
	};

	extern GUI gui;
}
