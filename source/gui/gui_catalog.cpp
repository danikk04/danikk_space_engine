#include <danikk_space_engine/gui.h>
#include <danikk_space_engine/asset.h>
#include <danikk_space_engine/item.h>
#include <danikk_space_engine/component/point/camera.h>
#include <danikk_space_engine/world/world.h>
#include <danikk_space_engine/world/build_part_shape.h>

#include <danikk_framework/glm.h>
#include <danikk_engine/texture_methods.h>

#include <limits>

#include "gui_internal.h"

namespace danikk_space_engine
{
	namespace
	{
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

	/** @brief Refreshes visible tool commands and category highlighting. */
	void GUI::refreshToolCatalog()
	{
		for(uint32 index = 0; index < catalog_category_buttons.size(); ++index)
		{
			catalog_category_buttons[index].draw_style =
				index == selected_tool_category
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
}
