#include <danikk_space_engine/gui/gui.h>
#include <danikk_space_engine/assets/collection.h>
#include <danikk_framework/glm.h>
#include <danikk_engine/input.h>

#include <danikk_space_engine/gui/gui_internal.h>

namespace danikk_space_engine
{
	GUI gui;

	namespace
	{
		constexpr float hotbar_slot_height = 0.070f;
		constexpr float hotbar_slot_gap = 0.0045f;
		constexpr float hotbar_bottom_margin = 0.025f;

		void writeHotbarEntryName(const void* source, String& text)
		{
			if(source == nullptr) return;
			const ToolCatalogEntryId entry_id = *static_cast<const ToolCatalogEntryId*>(source);
			if(entry_id == no_tool_catalog_entry) return;
			text.append(getToolCatalogEntry(entry_id).short_name);
		}
	}

	/** @copydoc GUI::define */
	void GUI::define()
	{
		message_log.text_style.shader = static_asset_collection.base2d_shader;
		message_log.text_style.font = static_asset_collection.base_font;
		message_log.text_style.anchor = TextAnchor::multiline;
		message_log.text_style.absolute_char_height = 0.014f;
		message_log.text_style.absolute_char_width = 0.009f;
		message_log.text_style.color = vec4(0.92f, 0.93f, 0.94f, 1.0f);
		message_log.text_style.max_char_count = 512;
		message_log.label.anchor = GUIAnchor::top_right;
		message_log.label.pos = vec2(0.025f, 0.025f);
		message_log.label.relative_size = vec2(0.36f, 0.60f);
		message_log.label.text_style = &message_log.text_style;
		message_log.label.name = "player_messages";
		message_log.label.flags.set(GUIElementFlags::visible, false);

		debug_overlay.text_style.anchor = TextAnchor::multiline;
		debug_overlay.text_style.absolute_char_height = 0.014f;
		debug_overlay.text_style.absolute_char_width = 0.014f;
		debug_overlay.text_style.shader = static_asset_collection.base2d_shader;
		debug_overlay.text_style.font = static_asset_collection.base_font;

		debug_overlay.label.anchor = GUIAnchor::top_left;
		debug_overlay.label.text_style = &debug_overlay.text_style;
		debug_overlay.label.pos = vec2(0, 0);
		debug_overlay.label.relative_size = vec2(0.4f, 0.3f);
		debug_overlay.label.name = "debug_info_label";

		hotbar.slot_fill_style.shader = static_asset_collection.base2d_shader;
		hotbar.slot_fill_style.color = vec4(0.20f, 0.21f, 0.22f, 1.0f);
		hotbar.slot_border_style.shader = static_asset_collection.base2d_shader;
		hotbar.slot_border_style.color = vec4(0.48f, 0.49f, 0.50f, 1.0f);
		hotbar.slot_border_style.width = getPixelSize() * 2.0f;
		hotbar.selected_slot_border_style.shader = static_asset_collection.base2d_shader;
		hotbar.selected_slot_border_style.color = vec4(0.78f, 0.79f, 0.80f, 1.0f);
		hotbar.selected_slot_border_style.width = getPixelSize() * 3.0f;
		hotbar.slot_text_style.shader = static_asset_collection.base2d_shader;
		hotbar.slot_text_style.font = static_asset_collection.base_font;
		hotbar.slot_text_style.anchor = TextAnchor::multiline;
		hotbar.slot_text_style.absolute_char_width = 0.005f;
		hotbar.slot_text_style.absolute_char_height = 0.010f;
		hotbar.slot_text_style.color = vec4(0.78f, 0.79f, 0.80f, 1.0f);
		hotbar.item_text_style.shader = static_asset_collection.base2d_shader;
		hotbar.item_text_style.font = static_asset_collection.base_font;
		hotbar.item_text_style.anchor = TextAnchor::center;
		hotbar.item_text_style.absolute_char_width = 0.004f;
		hotbar.item_text_style.absolute_char_height = 0.007f;
		hotbar.item_text_style.color = vec4(0.88f, 0.89f, 0.90f, 1.0f);
		selected_units.text_style.shader = static_asset_collection.base2d_shader;
		selected_units.text_style.font = static_asset_collection.base_font;
		selected_units.text_style.anchor = TextAnchor::multiline;
		selected_units.text_style.absolute_char_width = 0.008f;
		selected_units.text_style.absolute_char_height = 0.014f;
		selected_units.text_style.color = vec4(0.88f, 0.89f, 0.90f, 1.0f);

		shared_styles.panel_fill.shader = static_asset_collection.base2d_shader;
		shared_styles.panel_fill.color = vec4(0.12f, 0.13f, 0.14f, 0.97f);
		shared_styles.button_fill.shader = static_asset_collection.base2d_shader;
		shared_styles.button_fill.color = vec4(0.20f, 0.21f, 0.22f, 1.0f);
		shared_styles.selected_button_fill.shader = static_asset_collection.base2d_shader;
		shared_styles.selected_button_fill.color = vec4(0.32f, 0.34f, 0.36f, 1.0f);
		shared_styles.border.shader = static_asset_collection.base2d_shader;
		shared_styles.border.color = vec4(0.46f, 0.48f, 0.50f, 1.0f);
		shared_styles.border.width = getPixelSize() * 2.0f;
		tool_catalog.text_style.shader = static_asset_collection.base2d_shader;
		tool_catalog.text_style.font = static_asset_collection.base_font;
		tool_catalog.text_style.anchor = TextAnchor::center;
		tool_catalog.text_style.absolute_char_width = 0.009f;
		tool_catalog.text_style.absolute_char_height = 0.016f;
		tool_catalog.text_style.color = vec4(0.84f, 0.85f, 0.86f, 1.0f);
		tool_catalog.panel.anchor = GUIAnchor::down_left;
		tool_catalog.panel.pos = vec2(0.0f);
		tool_catalog.panel.relative_size = vec2(1.0f);
		tool_catalog.panel.name = "tool_catalog_panel";
		tool_catalog.panel.flags.set(GUIElementFlags::visible, false);
		if(getToolCategoryCount() > gui_detail::ToolCatalogWindow::category_capacity
			|| getToolCatalogEntryTotalCount() > gui_detail::ToolCatalogWindow::item_capacity)
		{
			danikk_engine::formatFatalError("Tool catalog exceeds GUI widget capacity");
		}
		tool_catalog.category_buttons.resize(getToolCategoryCount());
		tool_catalog.category_labels.resize(getToolCategoryCount());
		tool_catalog.item_buttons.resize(getToolCatalogEntryTotalCount());
		tool_catalog.item_labels.resize(getToolCatalogEntryTotalCount());
		const float hotbar_slot_width = hotbar_slot_height * getPixelSize().x / getPixelSize().y;
		const float horizontal_step = hotbar_slot_width + hotbar_slot_gap;
		constexpr float vertical_step = hotbar_slot_height + hotbar_slot_gap;
		const float leftmost_hotbar_cell_left = 0.5f
			- (gui_detail::Hotbar::column_count - 1) * 0.5f * horizontal_step - hotbar_slot_width * 0.5f;
		selected_units.panel.anchor = GUIAnchor::down_left;
		selected_units.panel.pos = vec2(hotbar_bottom_margin, hotbar_bottom_margin);
		selected_units.panel.relative_size = vec2(
			leftmost_hotbar_cell_left - hotbar_slot_gap - hotbar_bottom_margin,
			gui_detail::Hotbar::row_count * hotbar_slot_height + (gui_detail::Hotbar::row_count - 1) * hotbar_slot_gap);
		selected_units.panel.draw_style = &shared_styles.panel_fill;
		selected_units.panel.border_draw_style = &shared_styles.border;
		selected_units.panel.name = "selected_units_panel";
		selected_units.panel.flags.set(GUIElementFlags::visible, false);
		selected_units.label.anchor = GUIAnchor::center;
		selected_units.label.relative_size = vec2(0.90f, 0.90f);
		selected_units.label.text_style = &selected_units.text_style;
		selected_units.label.name = "selected_units_summary";
		selected_units.panel.addChild(selected_units.label);
		const uint32 catalog_rows = (getToolCatalogEntryTotalCount() + gui_detail::Hotbar::column_count - 1)
			/ gui_detail::Hotbar::column_count;
		const float catalog_bottom = hotbar_bottom_margin + gui_detail::Hotbar::row_count * vertical_step;
		const float tabs_bottom = catalog_bottom + catalog_rows * vertical_step;

		tool_catalog.close_button.anchor = GUIAnchor::down;
		tool_catalog.close_button.pos = vec2((gui_detail::Hotbar::column_count - 1) * 0.5f * horizontal_step, tabs_bottom);
		tool_catalog.close_button.relative_size = vec2(hotbar_slot_width, hotbar_slot_height * 0.5f);
		tool_catalog.close_button.draw_style = &shared_styles.button_fill;
		tool_catalog.close_button.border_draw_style = &shared_styles.border;
		tool_catalog.close_button.name = "tool_catalog_close";
		tool_catalog.close_label.anchor = GUIAnchor::center;
		tool_catalog.close_label.relative_size = vec2(1.0f);
		tool_catalog.close_label.text = "X";
		tool_catalog.close_label.text_style = &tool_catalog.text_style;
		tool_catalog.close_button.on_click = [this]()
		{
			queueAction(GUIActionType::close_tool_catalog);
			tool_catalog.panel.flags.set(GUIElementFlags::visible, false);
			setCursorCaptured(true);
		};

		for(uint32 index = 0; index < getToolCategoryCount(); ++index)
		{
			GUIElement& button = tool_catalog.category_buttons[index];
			GUIElement& label = tool_catalog.category_labels[index];
			button.anchor = GUIAnchor::down;
			const float tab_width = hotbar_slot_width * 2.0f + hotbar_slot_gap;
			button.pos = vec2((static_cast<float>(index) * 2.0f
				- (gui_detail::Hotbar::column_count - 2) * 0.5f) * horizontal_step, tabs_bottom);
			button.relative_size = vec2(tab_width, hotbar_slot_height * 0.5f);
			button.draw_style = &shared_styles.button_fill;
			button.border_draw_style = &shared_styles.border;
			button.name = "tool_catalog_category";
			label.anchor = GUIAnchor::center;
			label.relative_size = vec2(1.0f);
			label.text = getToolCategory(index).name;
			label.text_style = &tool_catalog.text_style;
			label.name = "tool_catalog_category_label";
			button.on_click = [this, index]()
			{
				tool_catalog.selected_category = index;
				refreshToolCatalog();
			};
		}

		for(uint32 index = 0; index < tool_catalog.item_buttons.size(); ++index)
		{
			GUIElement& button = tool_catalog.item_buttons[index];
			GUIElement& label = tool_catalog.item_labels[index];
			button.anchor = GUIAnchor::down;
			button.pos = vec2(
				(static_cast<float>(index % gui_detail::Hotbar::column_count) - (gui_detail::Hotbar::column_count - 1) * 0.5f)
					* horizontal_step,
				catalog_bottom + (index / gui_detail::Hotbar::column_count) * vertical_step);
			button.relative_size = vec2(hotbar_slot_width, hotbar_slot_height);
			button.draw_style = &shared_styles.button_fill;
			button.border_draw_style = &shared_styles.border;
			button.name = "tool_catalog_item";
			label.anchor = GUIAnchor::down;
			label.pos = vec2(0.0f, 0.04f);
			label.relative_size = vec2(1.0f, 0.36f);
			label.text_style = &hotbar.item_text_style;
			label.name = "tool_catalog_item_label";
			button.on_click = [this, index]()
			{
				if(index >= getToolCatalogEntryCount(tool_catalog.selected_category)) return;
				queueAction(GUIActionType::select_tool_catalog_entry,
					getToolCatalogEntryId(tool_catalog.selected_category, index));
				tool_catalog.panel.flags.set(GUIElementFlags::visible, false);
				setCursorCaptured(true);
				refreshToolCatalog();
			};
			button.draw_callback = [this, index](vec2 position, vec2 size)
			{
				if(index >= getToolCatalogEntryCount(tool_catalog.selected_category)) return;
				drawToolCatalogPreview(
					tool_catalog.preview_camera_front, tool_catalog.preview_camera_up, tool_catalog.preview_vertical_fov_degrees,
					getToolCatalogEntryId(tool_catalog.selected_category, index), position, size);
			};
		}

		for(uint32 row = 0; row < gui_detail::Hotbar::row_count; ++row)
		{
			for(uint32 column = 0; column < gui_detail::Hotbar::column_count; ++column)
			{
				const uint32 index = row * gui_detail::Hotbar::column_count + column;
				GUIElement& slot = hotbar.slots[index];
				GUIElement& label = hotbar.slot_labels[index];
				slot.anchor = GUIAnchor::down;
				slot.pos = vec2(
					(static_cast<float>(column) - (gui_detail::Hotbar::column_count - 1) * 0.5f) * horizontal_step,
					hotbar_bottom_margin + row * vertical_step);
				slot.relative_size = vec2(hotbar_slot_width, hotbar_slot_height);
				slot.draw_style = &hotbar.slot_fill_style;
				slot.border_draw_style = &hotbar.slot_border_style;
				slot.name = "hotbar_slot";

				label.anchor = GUIAnchor::top_left;
				label.pos = vec2(0.08f, 0.08f);
				label.relative_size = vec2(1.0f);
				label.text_style = &hotbar.slot_text_style;
				label.name = "hotbar_slot_label";
				const char* modifier = "";
				switch(row)
				{
					case 1: modifier = "Shift+\n"; break;
					case 2: modifier = "Ctrl+\n"; break;
				}
				label.text << modifier << "1234567890-+"[column];

				GUIElement& item_label = hotbar.item_labels[index];
				item_label.anchor = GUIAnchor::down;
				item_label.pos = vec2(0.0f, 0.04f);
				item_label.relative_size = vec2(1.0f, 0.36f);
				item_label.text_style = &hotbar.item_text_style;
				item_label.name = "hotbar_item_label";

				slot.on_click = [this, index]()
				{
					if(hotbar.catalog_open_source != nullptr && *hotbar.catalog_open_source)
					{
						queueAction(GUIActionType::select_hotbar_slot, no_tool_catalog_entry, index);
						refreshToolCatalog();
					}
				};
			}
		}
		for(GUIElement& slot : hotbar.slots)
		{
			slot.border_draw_style = &hotbar.slot_border_style;
		}
	}

	void GUI::bindDebugInfo(const void* source, GUIElement::TextWriter writer)
	{
		debug_overlay.source = gui_detail::TextSource{source, writer};
	}

	void GUI::bindSelectedUnits(const void* source, GUIElement::TextWriter writer,
		const void* visibility_source, bool (*visibility_reader)(const void*))
	{
		selected_units.has_units = visibility_reader;
		selected_units.visibility_source = visibility_source;
		selected_units.label.bind(source, writer);
	}

	void GUI::bindHotbar(const ToolCatalogEntryId* entries, const uint32* selected_slot,
		const bool* catalog_open)
	{
		hotbar.entry_ids_source = entries;
		hotbar.selected_slot_source = selected_slot;
		hotbar.catalog_open_source = catalog_open;
		for(uint32 index = 0; index < gui_detail::Hotbar::column_count * gui_detail::Hotbar::row_count; ++index)
			hotbar.item_labels[index].bind(entries == nullptr ? nullptr : entries + index,
				writeHotbarEntryName);
	}

	void GUI::bindPreviewCamera(const void* source,
		void (*reader)(const void*, vec3&, vec3&, float&))
	{
		tool_catalog.preview_camera_source = source;
		tool_catalog.read_preview_camera = reader;
		if(tool_catalog.read_preview_camera != nullptr)
			tool_catalog.read_preview_camera(tool_catalog.preview_camera_source, tool_catalog.preview_camera_front, tool_catalog.preview_camera_up,
				tool_catalog.preview_vertical_fov_degrees);
	}

	/** @copydoc GUI::queueAction */
	void GUI::queueAction(GUIActionType type, ToolCatalogEntryId entry_id, uint32 hotbar_slot)
	{
		pending_actions.push(GUIAction{type, entry_id, hotbar_slot});
	}

	/** @copydoc GUI::compose */
	void GUI::compose()
	{
		gui_root.addChild(debug_overlay.label);
		gui_root.addChild(selected_units.panel);
		for(GUIElement& slot : hotbar.slots)
		{
			gui_root.addChild(slot);
		}
		for(uint32 index = 0; index < gui_detail::Hotbar::column_count * gui_detail::Hotbar::row_count; ++index)
		{
			hotbar.slots[index].addChild(hotbar.slot_labels[index]);
			hotbar.slots[index].addChild(hotbar.item_labels[index]);
		}
		gui_root.addChild(tool_catalog.panel);
		tool_catalog.panel.addChild(tool_catalog.close_button);
		tool_catalog.close_button.addChild(tool_catalog.close_label);
		for(uint32 index = 0; index < tool_catalog.category_buttons.size(); ++index)
		{
			tool_catalog.panel.addChild(tool_catalog.category_buttons[index]);
			tool_catalog.category_buttons[index].addChild(tool_catalog.category_labels[index]);
		}
		for(uint32 index = 0; index < tool_catalog.item_buttons.size(); ++index)
		{
			tool_catalog.panel.addChild(tool_catalog.item_buttons[index]);
			tool_catalog.item_buttons[index].addChild(tool_catalog.item_labels[index]);
		}
		gui_root.addChild(message_log.label);
	}
}
