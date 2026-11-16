#pragma once

#include <danikk_engine/gui.h>
#include <danikk_engine/fps.h>
#include <danikk_engine/style.h>
#include <danikk_space_engine/default.h>

namespace danikk_space_engine
{

	/**
	 * @brief Owns the top-left debug label and periodically formats live runtime values.
	 *
	 * The GUI text reports FPS, process memory, active tool/action, camera speed,
	 * and camera position. Process memory is sampled at roughly one-second
	 * intervals; a zero reading is displayed as unavailable.
	 */
	class GUI
	{
		/** @brief Number of columns in the visual build hotbar. */
		static constexpr uint32 hotbar_column_count = 12;
		/** @brief Number of rows in the visual build hotbar. */
		static constexpr uint32 hotbar_row_count = 3;
		/** @brief Empty toolbar slot elements, laid out row by row from the bottom. */
		GUIElement hotbar_slots[hotbar_column_count * hotbar_row_count];
		/** @brief Small keyboard shortcut labels drawn above each slot's background. */
		GUIElement hotbar_slot_labels[hotbar_column_count * hotbar_row_count];
		/** @brief Shared dark-gray fill style for every empty toolbar slot. */
		DrawStyle hotbar_slot_fill_style;
		/** @brief Shared thin gray outline style for every toolbar slot. */
		BorderDrawStyle hotbar_slot_border_style;
		/** @brief Brighter gray outline style for the currently selected toolbar slot. */
		BorderDrawStyle hotbar_selected_slot_border_style;
		/** @brief Shared small-font style for toolbar keyboard shortcut labels. */
		TextStyle hotbar_slot_text_style;
		/** @brief Engine GUI element used to display the game's multiline debug readout. */
		GUIElement debug_info_label;
		/** @brief Most recently sampled process memory usage in bytes; zero means unavailable. */
		size_t memory_sample_bytes = 0;
		/** @brief Elapsed frame time since the previous process-memory sample. */
		float memory_sample_timer = 1.0f;
	public:

		/**
		 * @brief Configures the debug label and positions the empty three-row build hotbar.
		 *
		 * The debug label uses the shared bitmap font and 0.014 screen-relative character dimensions.
		 * The hotbar contains twelve columns and three rows of gray slots, centered near the bottom
		 * edge. Its bottom row shows number symbols, middle row shows Shift shortcuts, and top row
		 * shows Ctrl shortcuts. Slot and label styles are shared by all 36 cells.
		 */
		void define();

		/**
		 * @brief Adds the configured debug label to the engine GUI root.
		 * Call after define() so the root receives the configured text style and anchor.
		 */
		void compose();

		/**
		 * @brief Refreshes sampled memory and debug text for the current application frame.
		 *
		 * Samples process memory when the accumulated target-frame delay reaches one second, then
		 * builds one multiline string in this order: FPS, RAM in MiB, active tool, selected action,
		 * camera speed in m/s, and camera position truncated to hundredths. A zero memory sample is
		 * shown as unavailable. Rendering of the label is driven by the surrounding engine frame loop.
		 */
		void frame();
	};

	extern GUI gui;
}
