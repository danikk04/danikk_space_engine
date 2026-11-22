#pragma once

#include <danikk_space_engine/gui.h>

namespace danikk_space_engine
{
	/** @brief Draws the miniature associated with one tool-catalog entry. */
	void drawToolCatalogPreview(const vec3& camera_front, const vec3& camera_up,
		float vertical_fov_degrees, ToolCatalogEntryId entry_id,
		const vec2& cell_position, const vec2& cell_size);
}
