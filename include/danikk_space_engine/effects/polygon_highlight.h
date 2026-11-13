#pragma once

#include <danikk_space_engine/world/volume_cell.h>
#include <danikk_space_engine/world/face_ptr.h>
#include <danikk_space_engine/fwd.h>

#include <danikk_engine/mesh.h>
#include <danikk_framework/glm.h>

namespace danikk_space_engine
{
	void drawPolygonHighlight(
		const World& world,
		const FacePtr& face,
		Mesh& highlight_mesh,
		const vec4& color);

	void drawPolygonHighlights(const World& world);
}
