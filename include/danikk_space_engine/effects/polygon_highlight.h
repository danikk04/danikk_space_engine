#pragma once

#include <danikk_space_engine/world/face_ptr.h>
#include <danikk_space_engine/fwd.h>

#include <danikk_engine/mesh.h>
#include <danikk_framework/glm.h>

namespace danikk_space_engine
{

	/**
	 * @brief Draws one editable triangle as a camera-offset, slightly inset overlay.
	 * @param world World used to validate the face and obtain its transforms.
	 * @param face Stable chunk/triangle reference to highlight.
	 * @param highlight_mesh Reusable mesh receiving the triangle overlay geometry.
	 * @param color Solid overlay color.
	 * Planet chunks, inactive resources, invalid points, and a missing camera point
	 * are skipped. Each vertex is moved toward the triangle centroid by dividing its
	 * offset by 1.05, then the result is translated 0.002 world units toward the camera
	 * before upload. This reduces overlap with the surface and limits depth fighting.
	 */
	void drawPolygonHighlight(
		const World& world,
		const FacePtr& face,
		Mesh& highlight_mesh,
		const vec4& color);

	/**
	 * @brief Draws all selected faces and the current hovered face from the local controller.
	 * @param world World providing the controller selection and geometry.
	 * Entries before selected_face_count use gray `(0.5, 0.5, 0.5, 1)`; any remaining
	 * final entry is the hovered face and uses red `(1, 0.15, 0.15, 1)`. Planet terrain
	 * is intentionally excluded from this effect.
	 */
	void drawPolygonHighlights(const World& world);
}
