#pragma once

#include <danikk_space_engine/gui.h>

namespace danikk_space_engine
{
	/** @brief Draws the miniature associated with one tool-catalog entry. */
	void drawToolCatalogPreview(const GUIFrameData& frame_data, ToolCatalogEntryId entry_id,
		const vec2& cell_position, const vec2& cell_size);
}
