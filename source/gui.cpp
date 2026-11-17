#include <danikk_space_engine/gui.h>
#include <danikk_space_engine/asset.h>
#include <danikk_space_engine/item.h>
#include <danikk_space_engine/component/point/camera.h>
#include <danikk_space_engine/world/world.h>
#include <danikk_space_engine/world/build_part_shape.h>

#include <danikk_framework/glm.h>
#include <danikk_framework/format.h>
#include <danikk_framework/memory.h>
#include <danikk_engine/input.h>
#include <danikk_engine/texture_methods.h>

#include <limits>

namespace danikk_space_engine
{
	GUI gui;

	namespace
	{
		constexpr float hotbar_slot_height = 0.070f;
		constexpr float hotbar_slot_gap = 0.0045f;
		Mesh build_part_preview_meshes[static_cast<size_t>(BuildPartShape::count)];
		bool build_part_preview_meshes_initialized = false;

	/** @brief Builds cached GPU meshes used by the catalog's cube and slope thumbnails. */
	void initializeBuildPartPreviewMeshes()
	{
		if(build_part_preview_meshes_initialized) return;
		for(uint32 shape_index = 0; shape_index < static_cast<uint32>(BuildPartShape::count); ++shape_index)
		{
			const BuildPartGeometry& geometry = getBuildPartGeometry(static_cast<BuildPartShape>(shape_index));
			DynamicArray<DefaultVertex> vertices;
			DynamicArray<gl_point_index_t> indices;
			vertices.reserve(geometry.triangles.size() * 3);
			indices.reserve(geometry.triangles.size() * 3);
			for(const CubeTriangle& triangle : geometry.triangles)
			{
				const vec3 a = geometry.positions[triangle.points[0]];
				const vec3 b = geometry.positions[triangle.points[1]];
				const vec3 c = geometry.positions[triangle.points[2]];
				const vec3 normal = glm::normalize(glm::cross(b - a, c - a));
				for(const vec3& position : {a, b, c})
				{
					indices.push(static_cast<gl_point_index_t>(vertices.size()));
					vertices.pushCtor(position, normal, vec2(0.0f));
				}
			}
			Mesh& mesh = build_part_preview_meshes[shape_index];
			mesh.setData(reinterpret_cast<const float*>(vertices.data()),
				vertices.size() * sizeof(DefaultVertex) / sizeof(float), indices.data(), indices.size());
			DefaultVertex::setAttributes();
		}
		build_part_preview_meshes_initialized = true;
	}

	/**
	 * @brief Draws a lit miniature of a build definition centered over its catalog cell.
	 * @param definition_id Build item represented by the cell.
	 * @param cell_position Bottom-left GUI position of the cell.
	 * @param cell_size GUI dimensions of the cell.
	 * The preview is placed one unit in front of the camera and uses the active camera
	 * projection, so it occupies the same screen rectangle as the cell without changing
	 * the projection or view uniforms used for the next world frame. BuildPart meshes
	 * use shared primitive geometry; the weapon thumbnail draws the catalogued MG-42 mesh.
	 */
	void drawToolCatalogPreview(ToolCatalogEntryId entry_id, const vec2& cell_position, const vec2& cell_size)
	{
		const ToolCatalogEntry& entry = getToolCatalogEntry(entry_id);
		const CameraComponent& camera = world.getCameraComponent();
		const float depth = 1.0f;
		const float half_height = tan(glm::radians(camera.vertical_fov_degrees) * 0.5f) * depth;
		const float half_width = half_height * screen_ratio_gz;
		const vec3 view_center(
			(2.0f * (cell_position.x + cell_size.x * 0.5f) - 1.0f) * half_width,
			(2.0f * (cell_position.y + cell_size.y * 0.5f) - 1.0f) * half_height,
			-depth);
		const mat4 view = glm::lookAt(vec3(0.0f), camera.getFront(), camera.getUp());
		const mat4 camera_world = glm::inverse(view);
		const mat4 cell_transform = camera_world * glm::translate(mat4(1.0f), view_center);
		static_asset_collection.base3d_shader.use();

		if(entry.action == ToolActionId::create_body || entry.action == ToolActionId::create_slope)
		{
			initializeBuildPartPreviewMeshes();
			const BuildPartShape shape = entry.action == ToolActionId::create_slope
				? BuildPartShape::slope : BuildPartShape::cube;
			const vec3 view_size(
				2.0f * half_width * cell_size.x * 0.76f,
				2.0f * half_height * cell_size.y * 0.76f,
				2.0f * half_height * cell_size.y * 0.76f);
			const mat4 rotation = glm::rotate(mat4(1.0f), glm::radians(24.0f), vec3(1.0f, 0.0f, 0.0f))
				* glm::rotate(mat4(1.0f), glm::radians(-32.0f), vec3(0.0f, 1.0f, 0.0f))
				* glm::rotate(mat4(1.0f), glm::radians(28.0f), vec3(0.0f, 0.0f, 1.0f));
			setWorldMatrix(cell_transform * glm::scale(mat4(1.0f), view_size) * rotation);
			white_texture.bind();
			setDrawColor(vec4(0.78f, 0.82f, 0.86f, 1.0f));
			build_part_preview_meshes[static_cast<size_t>(shape)].draw();
			return;
		}

		if(entry.action == ToolActionId::machinegun)
		{
			const mat4 rotation = glm::rotate(mat4(1.0f), glm::radians(58.0f), vec3(1.0f, 0.0f, 0.0f))
				* glm::rotate(mat4(1.0f), glm::radians(-18.0f), vec3(0.0f, 1.0f, 0.0f))
				* glm::rotate(mat4(1.0f), glm::radians(38.0f), vec3(0.0f, 0.0f, 1.0f));
			const vec3 model_center(0.001f, 0.039f, -0.241f);
			const vec3 model_half_extents(4.55f, 0.85f, 0.80f);
			float projected_min_x = std::numeric_limits<float>::max();
			float projected_max_x = std::numeric_limits<float>::lowest();
			float projected_min_y = std::numeric_limits<float>::max();
			float projected_max_y = std::numeric_limits<float>::lowest();
			for(int32 x_sign : {-1, 1})
			{
				for(int32 y_sign : {-1, 1})
				{
					for(int32 z_sign : {-1, 1})
					{
						const vec3 corner = model_center + model_half_extents
							* vec3(static_cast<float>(x_sign), static_cast<float>(y_sign), static_cast<float>(z_sign));
						const vec3 rotated = vec3(rotation * vec4(corner - model_center, 0.0f));
						projected_min_x = glm::min(projected_min_x, rotated.x);
						projected_max_x = glm::max(projected_max_x, rotated.x);
						projected_min_y = glm::min(projected_min_y, rotated.y);
						projected_max_y = glm::max(projected_max_y, rotated.y);
					}
				}
			}
			const float view_width = 2.0f * half_width * cell_size.x * 0.76f;
			const float view_height = 2.0f * half_height * cell_size.y * 0.76f;
			const float projected_width = projected_max_x - projected_min_x;
			const float projected_height = projected_max_y - projected_min_y;
			const float scale = glm::min(view_width / projected_width, view_height / projected_height);
			const mat4 item_transform = cell_transform * rotation
				* glm::scale(mat4(1.0f), vec3(scale))
				* glm::translate(mat4(1.0f), -model_center);
			drawItem(item_definition_ids::mg42, item_transform);
		}
	}
	}

	/** @brief Shared style referenced by the debug label for its lifetime. */
	TextStyle debug_info_label_style;

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
		catalog_panel.anchor = GUIAnchor::center;
		catalog_panel.pos = vec2(0.0f, 0.08f);
		catalog_panel.relative_size = vec2(0.68f, 0.60f);
		catalog_panel.draw_style = &catalog_panel_style;
		catalog_panel.border_draw_style = &catalog_border_style;
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
		const vec2 catalog_item_relative_size = vec2(hotbar_slot_width, hotbar_slot_height)
			/ catalog_panel.relative_size;
		const float catalog_item_gap = hotbar_slot_gap / catalog_panel.relative_size.x;

		catalog_close_button.anchor = GUIAnchor::top_right;
		catalog_close_button.pos = vec2(0.04f, 0.04f);
		catalog_close_button.relative_size = vec2(0.09f, 0.13f);
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
			button.anchor = GUIAnchor::top_left;
			button.pos = vec2(0.04f, 0.22f + index * 0.18f);
			button.relative_size = vec2(0.23f, 0.13f);
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
				selected_tool_category = getToolCategory(index).id;
				refreshToolCatalog();
			};
		}

		for(uint32 index = 0; index < catalog_item_buttons.size(); ++index)
		{
			GUIElement& button = catalog_item_buttons[index];
			GUIElement& label = catalog_item_labels[index];
			button.anchor = GUIAnchor::top_left;
			button.pos = vec2(0.32f + index * (catalog_item_relative_size.x + catalog_item_gap), 0.24f);
			button.relative_size = catalog_item_relative_size;
			button.draw_style = &catalog_button_style;
			button.border_draw_style = &catalog_border_style;
			button.name = "tool_catalog_item";
			label.anchor = GUIAnchor::center;
			label.relative_size = vec2(1.0f);
			label.text_style = &catalog_text_style;
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

		constexpr float bottom_margin = 0.025f;
		const float horizontal_step = hotbar_slot_width + hotbar_slot_gap;
		constexpr float vertical_step = hotbar_slot_height + hotbar_slot_gap;

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
					bottom_margin + row * vertical_step);
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

	/** @brief Refreshes visible tool commands and category highlighting. */
	void GUI::refreshToolCatalog()
	{
		for(uint32 index = 0; index < catalog_category_buttons.size(); ++index)
		{
			catalog_category_buttons[index].draw_style =
				getToolCategory(index).id == selected_tool_category
					? &catalog_selected_button_style : &catalog_button_style;
		}
		const uint32 item_count = getToolCatalogEntryCount(selected_tool_category);
		const LocalPlayerControllerComponent& controller = world.getLocalPlayerControllerComponent();
		const ToolCatalogEntryId selected_entry_id = controller.hotbar_entry_ids[controller.selected_hotbar_slot];
		for(uint32 index = 0; index < catalog_item_buttons.size(); ++index)
		{
			const bool has_item = index < item_count;
			catalog_item_buttons[index].flags.set(GUIElementFlags::visible, has_item);
			if(!has_item) continue;
			const ToolCatalogEntryId entry_id = getToolCatalogEntryId(selected_tool_category, index);
			catalog_item_labels[index].text = getToolCatalogEntry(entry_id).short_name;
			catalog_item_buttons[index].draw_style = entry_id == selected_entry_id
				? &catalog_selected_button_style : &catalog_button_style;
		}
	}

	/**
	 * @brief Refreshes the one-second memory sample and appends each debug value on its own line.
	 *
	 * A zero memory reading is formatted as unavailable. The camera coordinates
	 * are truncated to two decimal places before formatting. The completed
	 * framework String replaces the label text once after all fields are appended. Values are
	 * ordered as FPS, RAM, optional BuildPart controls, camera speed, and camera position.
	 */
	void GUI::frame()
	{
		memory_sample_timer += getTargetFrameDelay();
		if(memory_sample_timer >= 1.0f)
		{
			memory_sample_bytes = getProcessMemoryUsageBytes();
			memory_sample_timer = 0.0f;
		}

		const glm::dvec3 camera_position = truncateTo100(world.getCameraWorldPosition());
		LocalPlayerControllerComponent& controller = world.getLocalPlayerControllerComponent();
		if(getKeyboardState(keyboard_buttons::g) == button_states::press)
		{
			controller.tool_catalog_open = !controller.tool_catalog_open;
			refreshToolCatalog();
		}
		catalog_panel.flags.set(GUIElementFlags::visible, controller.tool_catalog_open);
		for(uint32 index = 0; index < hotbar_column_count * hotbar_row_count; ++index)
		{
			const ToolCatalogEntryId entry_id = controller.hotbar_entry_ids[index];
			hotbar_item_labels[index].text = entry_id == no_tool_catalog_entry
				? "" : getToolCatalogEntry(entry_id).short_name;
			hotbar_slots[index].border_draw_style = index == controller.selected_hotbar_slot
				? &hotbar_selected_slot_border_style
				: &hotbar_slot_border_style;
		}
		String text;
		formatAppend(text, "FPS:%\n", fps_counter.getValue());
		const size_t whole_mebibytes = memory_sample_bytes / mib(1);
		const size_t tenths_of_mebibyte = (memory_sample_bytes % mib(1)) * 10 / mib(1);
		formatAppend(text,
			memory_sample_bytes == 0 ? "RAM: unavailable\n" : "RAM:%.% MiB\n",
			whole_mebibytes, tenths_of_mebibyte);

		if(controller.active_tool == ToolId::place
			&& (controller.selected_tool_action == ToolActionId::create_body
				|| controller.selected_tool_action == ToolActionId::create_slope))
		{
			const char* size_axis_name = controller.build_part_size_axis == 0 ? "X"
				: controller.build_part_size_axis == 1 ? "Y" : "Z";
			formatAppend(text, "BUILD SIZE:% x % x % m\n",
				controller.build_part_size.x,
				controller.build_part_size.y,
				controller.build_part_size.z);
			formatAppend(text, "SIZE AXIS:% (X/Y/Z, Shift+Wheel)\n", size_axis_name);
		}
		formatAppend(text, "CAMERA SPEED:% m/s\n", controller.camera_movement_speed);
		formatAppend(text, "camera_pos:%", camera_position);
		debug_info_label.text = static_cast<String&&>(text);
	}
}
