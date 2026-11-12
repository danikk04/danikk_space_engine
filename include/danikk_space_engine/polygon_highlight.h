#pragma once

#include <danikk_space_engine/volume_cell.h>
#include <danikk_space_engine/fwd.h>

#include <danikk_engine/mesh.h>
#include <danikk_framework/glm.h>

namespace danikk_space_engine
{
	void drawPolygonHighlight(
		const World& world,
		uint32 region_index,
		VolumeCellId cell_id,
		uint8 face_index,
		Mesh& highlight_mesh,
		const vec4& color);
}
