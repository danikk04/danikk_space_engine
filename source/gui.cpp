#include <danikk_space_engine/gui.h>
#include <danikk_space_engine/asset.h>
#include <danikk_space_engine/world/world.h>
#include <danikk_framework/glm.h>

#include "gui_internal.h"

namespace danikk_space_engine
{
	GUI gui;

	namespace
	{
		constexpr float hotbar_slot_height = 0.070f;
		constexpr float hotbar_slot_gap = 0.0045f;
		constexpr float hotbar_bottom_margin = 0.025f;
		TextStyle debug_info_label_style;
	}

	/** @copydoc GUI::define */
	void GUI::define()
	{
		debug_info_label_style.anchor = TextAnchor::multiline;
		debug_info_label_style.absolute_char_height = 0.014f;
		debug_info_label_style.absolute_char_width = 0.014f;
		debug_info_label_style.shader = static_asset_collection.base2d_shader;
		debug_info_label_style.font = static_asset_collection.base_font;

		debug_info_label.anchor = GUIAnchor::top_left;
		debug_info_label.text_style = &debug_info_label_style;
		debug_info_label.pos = vec2(0, 0);
		debug_info_label.relative_size = vec2(0.4f, 0.3f);
		debug_info_label.name = "debug_info_label";

		hotbar_slot_fill_style.shader = static_asset_collection.base2d_shader;
		hotbar_slot_fill_style.color = vec4(0.20f, 0.21f, 0.22f, 1.0f);
		hotbar_slot_border_style.shader = static_asset_collection.base2d_shader;
		hotbar_slot_border_style.color = vec4(0.48f, 0.49f, 0.50f, 1.0f);
		hotbar_slot_border_style.width = getPixelSize() * 2.0f;
		hotbar_selected_slot_border_style.shader = static_asset_collection.base2d_shader;
		hotbar_selected_slot_border_style.color = vec4(0.78f, 0.79f, 0.80f, 1.0f);
		hotbar_selected_slot_border_style.width = getPixelSize() * 3.0f;
		hotbar_slot_text_style.shader = static_asset_collection.base2d_shader;
		hotbar_slot_text_style.font = static_asset_collection.base_font;
		hotbar_slot_text_style.anchor = TextAnchor::multiline;
		hotbar_slot_text_style.absolute_char_width = 0.005f;
		hotbar_slot_text_style.absolute_char_height = 0.010f;
		hotbar_slot_text_style.color = vec4(0.78f, 0.79f, 0.80f, 1.0f);
		hotbar_item_text_style.shader = static_asset_collection.base2d_shader;
		hotbar_item_text_style.font = static_asset_collection.base_font;
		hotbar_item_text_style.anchor = TextAnchor::center;
		hotbar_item_text_style.absolute_char_width = 0.004f;
		hotbar_item_text_style.absolute_char_height = 0.007f;
		hotbar_item_text_style.color = vec4(0.88f, 0.89f, 0.90f, 1.0f);

		catalog_panel_style.shader = static_asset_collection.base2d_shader;
		catalog_panel_style.color = vec4(0.12f, 0.13f, 0.14f, 0.97f);
		catalog_button_style.shader = static_asset_collection.base2d_shader;
		catalog_button_style.color = vec4(0.20f, 0.21f, 0.22f, 1.0f);
		catalog_selected_button_style.shader = static_asset_collection.base2d_shader;
		catalog_selected_button_style.color = vec4(0.32f, 0.34f, 0.36f, 1.0f);
		catalog_border_style.shader = static_asset_collection.base2d_shader;
		catalog_border_style.color = vec4(0.46f, 0.48f, 0.50f, 1.0f);
		catalog_border_style.width = getPixelSize() * 2.0f;
		catalog_text_style.shader = static_asset_collection.base2d_shader;
		catalog_text_style.font = static_asset_collection.base_font;
		catalog_text_style.anchor = TextAnchor::center;
		catalog_text_style.absolute_char_width = 0.009f;
		catalog_text_style.absolute_char_height = 0.016f;
		catalog_text_style.color = vec4(0.84f, 0.85f, 0.86f, 1.0f);
		catalog_panel.anchor = GUIAnchor::down_left;
		catalog_panel.pos = vec2(0.0f);
		catalog_panel.relative_size = vec2(1.0f);
		catalog_panel.name = "tool_catalog_panel";
		catalog_panel.flags.set(GUIElementFlags::visible, false);
		if(getToolCategoryCount() > catalog_category_capacity
			|| getToolCatalogEntryTotalCount() > catalog_item_capacity)
		{
			danikk_engine::formatFatalError("Tool catalog exceeds GUI widget capacity");
		}
		catalog_category_buttons.resize(getToolCategoryCount());
		catalog_category_labels.resize(getToolCategoryCount());
		catalog_item_buttons.resize(getToolCatalogEntryTotalCount());
		catalog_item_labels.resize(getToolCatalogEntryTotalCount());
		const float hotbar_slot_width = hotbar_slot_height * getPixelSize().x / getPixelSize().y;
		const float horizontal_step = hotbar_slot_width + hotbar_slot_gap;
		constexpr float vertical_step = hotbar_slot_height + hotbar_slot_gap;
		const uint32 catalog_rows = (getToolCatalogEntryTotalCount() + hotbar_column_count - 1)
			/ hotbar_column_count;
		const float catalog_bottom = hotbar_bottom_margin + hotbar_row_count * vertical_step;
		const float tabs_bottom = catalog_bottom + catalog_rows * vertical_step;

		catalog_close_button.anchor = GUIAnchor::down;
		catalog_close_button.pos = vec2((hotbar_column_count - 1) * 0.5f * horizontal_step, tabs_bottom);
		catalog_close_button.relative_size = vec2(hotbar_slot_width, hotbar_slot_height * 0.5f);
		catalog_close_button.draw_style = &catalog_button_style;
		catalog_close_button.border_draw_style = &catalog_border_style;
		catalog_close_button.name = "tool_catalog_close";
		catalog_close_label.anchor = GUIAnchor::center;
		catalog_close_label.relative_size = vec2(1.0f);
		catalog_close_label.text = "X";
		catalog_close_label.text_style = &catalog_text_style;
		catalog_close_button.on_click = [this]()
		{
			world.getLocalPlayerControllerComponent().tool_catalog_open = false;
		};

		for(uint32 index = 0; index < getToolCategoryCount(); ++index)
		{
			GUIElement& button = catalog_category_buttons[index];
			GUIElement& label = catalog_category_labels[index];
			button.anchor = GUIAnchor::down;
			const float tab_width = hotbar_slot_width * 2.0f + hotbar_slot_gap;
			button.pos = vec2((static_cast<float>(index) * 2.0f
				- (hotbar_column_count - 2) * 0.5f) * horizontal_step, tabs_bottom);
			button.relative_size = vec2(tab_width, hotbar_slot_height * 0.5f);
			button.draw_style = &catalog_button_style;
			button.border_draw_style = &catalog_border_style;
			button.name = "tool_catalog_category";
			label.anchor = GUIAnchor::center;
			label.relative_size = vec2(1.0f);
			label.text = getToolCategory(index).name;
			label.text_style = &catalog_text_style;
			label.name = "tool_catalog_category_label";
			button.on_click = [this, index]()
			{
				selected_tool_category = index;
				refreshToolCatalog();
			};
		}

		for(uint32 index = 0; index < catalog_item_buttons.size(); ++index)
		{
			GUIElement& button = catalog_item_buttons[index];
			GUIElement& label = catalog_item_labels[index];
			button.anchor = GUIAnchor::down;
			button.pos = vec2(
				(static_cast<float>(index % hotbar_column_count) - (hotbar_column_count - 1) * 0.5f)
					* horizontal_step,
				catalog_bottom + (index / hotbar_column_count) * vertical_step);
			button.relative_size = vec2(hotbar_slot_width, hotbar_slot_height);
			button.draw_style = &catalog_button_style;
			button.border_draw_style = &catalog_border_style;
			button.name = "tool_catalog_item";
			label.anchor = GUIAnchor::down;
			label.pos = vec2(0.0f, 0.04f);
			label.relative_size = vec2(1.0f, 0.36f);
			label.text_style = &hotbar_item_text_style;
			label.name = "tool_catalog_item_label";
			button.on_click = [this, index]()
			{
				if(index >= getToolCatalogEntryCount(selected_tool_category)) return;
				LocalPlayerControllerComponent& controller = world.getLocalPlayerControllerComponent();
				controller.selectToolCatalogEntry(getToolCatalogEntryId(selected_tool_category, index));
				controller.tool_catalog_open = false;
				refreshToolCatalog();
			};
			button.draw_callback = [this, index](vec2 position, vec2 size)
			{
				if(index >= getToolCatalogEntryCount(selected_tool_category)) return;
				drawToolCatalogPreview(
					getToolCatalogEntryId(selected_tool_category, index), position, size);
			};
		}

		for(uint32 row = 0; row < hotbar_row_count; ++row)
		{
			for(uint32 column = 0; column < hotbar_column_count; ++column)
			{
				const uint32 index = row * hotbar_column_count + column;
				GUIElement& slot = hotbar_slots[index];
				GUIElement& label = hotbar_slot_labels[index];
				slot.anchor = GUIAnchor::down;
				slot.pos = vec2(
					(static_cast<float>(column) - (hotbar_column_count - 1) * 0.5f) * horizontal_step,
					hotbar_bottom_margin + row * vertical_step);
				slot.relative_size = vec2(hotbar_slot_width, hotbar_slot_height);
				slot.draw_style = &hotbar_slot_fill_style;
				slot.border_draw_style = &hotbar_slot_border_style;
				slot.name = "hotbar_slot";

				label.anchor = GUIAnchor::top_left;
				label.pos = vec2(0.08f, 0.08f);
				label.relative_size = vec2(1.0f);
				label.text_style = &hotbar_slot_text_style;
				label.name = "hotbar_slot_label";
				const char* modifier = "";
				switch(row)
				{
					case 1: modifier = "Shift+\n"; break;
					case 2: modifier = "Ctrl+\n"; break;
				}
				label.text << modifier << "1234567890-+"[column];

				GUIElement& item_label = hotbar_item_labels[index];
				item_label.anchor = GUIAnchor::down;
				item_label.pos = vec2(0.0f, 0.04f);
				item_label.relative_size = vec2(1.0f, 0.36f);
				item_label.text_style = &hotbar_item_text_style;
				item_label.name = "hotbar_item_label";

				slot.on_click = [this, index]()
				{
					LocalPlayerControllerComponent& controller = world.getLocalPlayerControllerComponent();
					if(controller.tool_catalog_open)
					{
						controller.selectHotbarSlot(index);
						refreshToolCatalog();
					}
				};
			}
		}
		for(GUIElement& slot : hotbar_slots)
		{
			slot.border_draw_style = &hotbar_slot_border_style;
		}
	}

	/** @copydoc GUI::compose */
	void GUI::compose()
	{
		gui_root.addChild(debug_info_label);
		for(GUIElement& slot : hotbar_slots)
		{
			gui_root.addChild(slot);
		}
		for(uint32 index = 0; index < hotbar_column_count * hotbar_row_count; ++index)
		{
			hotbar_slots[index].addChild(hotbar_slot_labels[index]);
			hotbar_slots[index].addChild(hotbar_item_labels[index]);
		}
		gui_root.addChild(catalog_panel);
		catalog_panel.addChild(catalog_close_button);
		catalog_close_button.addChild(catalog_close_label);
		for(uint32 index = 0; index < catalog_category_buttons.size(); ++index)
		{
			catalog_panel.addChild(catalog_category_buttons[index]);
			catalog_category_buttons[index].addChild(catalog_category_labels[index]);
		}
		for(uint32 index = 0; index < catalog_item_buttons.size(); ++index)
		{
			catalog_panel.addChild(catalog_item_buttons[index]);
			catalog_item_buttons[index].addChild(catalog_item_labels[index]);
		}
	}
}
