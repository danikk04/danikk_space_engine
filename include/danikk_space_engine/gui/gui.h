#pragma once

#include <danikk_engine/gui.h>
#include <danikk_engine/fps.h>
#include <danikk_engine/style.h>
#include <danikk_space_engine/default.h>
#include <danikk_space_engine/gui/data.h>
#include <danikk_space_engine/tool_catalog.h>

namespace danikk_space_engine
{
	/**
	 * @brief Owns the debug overlay, selected-unit summary, message list, hotbar, and tool catalog.
	 *
	 * The GUI text reports FPS, process memory, BuildPart size when applicable, camera speed,
	 * signed height above the nearest planet, and camera position. Process memory is sampled at roughly one-second
	 * intervals; a zero reading is displayed as unavailable.
	 */
	class GUI
	{
		gui_detail::MessageLog message_log;
		gui_detail::DebugOverlay debug_overlay;
		gui_detail::Hotbar hotbar;
		gui_detail::SelectedUnitsPanel selected_units;
		gui_detail::ToolCatalogWindow tool_catalog;
		gui_detail::SharedStyles shared_styles;
		/** @brief GUI input requests waiting for the app/controller to process. */
		DynamicArray<GUIAction> pending_actions;

		/** @brief Refreshes visible commands and category highlighting in the tool menu. */
		void refreshToolCatalog();
		/** @brief Adds an application-facing action to the pending queue. */
		void queueAction(GUIActionType type, ToolCatalogEntryId entry_id = no_tool_catalog_entry,
			uint32 hotbar_slot = 0);
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
		 * camera speed in m/s, signed nearest-planet height, and camera position truncated to hundredths. A zero memory sample is
		 * shown as unavailable. Bound sources provide current world values without making the GUI
		 * depend on World or requiring a copied per-frame data structure. GUI input is queued as actions
		 * for the application to apply to the controller.
		 */
		void frame();

		/** @brief Binds game-specific debug values appended after FPS and memory. */
		void bindDebugInfo(const void* source, GUIElement::TextWriter writer);
		/** @brief Binds the selected-unit label and its panel visibility predicate. */
		void bindSelectedUnits(const void* source, GUIElement::TextWriter writer,
			const void* visibility_source, bool (*visibility_reader)(const void*));
		/** @brief Binds controller-owned hotbar contents and live selection/catalog state. */
		void bindHotbar(const ToolCatalogEntryId* entries, const uint32* selected_slot,
			const bool* catalog_open);
		/** @brief Binds the camera reader used by tool-catalog preview drawing. */
		void bindPreviewCamera(const void* source,
			void (*reader)(const void*, vec3&, vec3&, float&));

		/** @brief Returns queued actions for the app/controller to process. */
		const DynamicArray<GUIAction>& getPendingActions() const { return pending_actions; }
		/** @brief Removes all actions already processed by the app/controller. */
		void clearPendingActions() { pending_actions.clear(); }

		/**
		 * @brief Adds a player-facing message at the top of the on-screen list.
		 * @param text Message text; the message remains visible for ten seconds.
		 */
		void addMessage(const String& text);
	};

	extern GUI gui;
}
