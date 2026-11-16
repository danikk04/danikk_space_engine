#include <danikk_space_engine/effects/polygon_highlight.h>

#include <danikk_space_engine/world/world.h>
#include <danikk_space_engine/component/world_body/planet/planet.h>
#include <danikk_engine/texture_methods.h>

namespace danikk_space_engine
{
	namespace
	{

		/**
		 * @brief Reports whether planet chunk.
		 */
		bool isPlanetChunk(const World& world, const Chunk& chunk)
		{
			const WorldBody& body = world.data.getWorldBody(chunk.world_body_id);
			for(WorldBodyComponentSlotId slot_id : body.component_slot_ids)
			{
				if(!world.data.isWorldBodyComponentSlotActive(slot_id)) continue;
				if(world.data.getWorldBodyComponentSlot(slot_id).type_id == WorldBodyComponentTypeId::planet)
				{
					return true;
				}
			}
			return false;
		}
	}

	// Draws a selected mesh triangle as a slightly inset overlay toward the camera.
	// The overlay is omitted for generated planet terrain, which has no editable face selection.

	/**
	 * @brief Draws polygon highlight.
	 */
	void drawPolygonHighlight(
		const World& world,
		const FacePtr& face,
		Mesh& highlight_mesh,
		const vec4& color)
	{
		if(!world.data.isChunkActive(face.chunk_id)
			|| !world.data.isGroundTriangleActive(face.ground_triangle_id)) return;
		const Chunk& chunk = world.data.getChunk(face.chunk_id);
		if(isPlanetChunk(world, chunk)) return;
		const GroundTriangle& triangle = world.data.getGroundTriangle(face.ground_triangle_id);
		for(PointId point_id : triangle.points)
		{
			if(!world.data.isPointActive(point_id)) return;
		}
		if(!world.data.isPointActive(world.getCameraSlot().point_id)) return;

		const vec3 a = world.data.getPoint(triangle.points[0]).position;
		const vec3 b = world.data.getPoint(triangle.points[1]).position;
		const vec3 c = world.data.getPoint(triangle.points[2]).position;
		const vec3 center = (a + b + c) / 3.0f;
		const mat4 chunk_world_matrix = chunk.createWorldMatrix(world);
		const vec3 center_world = vec3(chunk_world_matrix * vec4(center, 1.0f));
		const vec3 camera_delta = -center_world;
		const float camera_distance = glm::length(camera_delta);
		const vec3 camera_direction = camera_distance > 0.000001f
			? camera_delta / camera_distance
			: vec3(0.0f);
		constexpr float highlight_offset = 0.002f;
		constexpr float shrink_factor = 1.05f;
		const vec3 positions[3]
		{
			center + (a - center) / shrink_factor + camera_direction * highlight_offset,
			center + (b - center) / shrink_factor + camera_direction * highlight_offset,
			center + (c - center) / shrink_factor + camera_direction * highlight_offset
		};
		const vec3 cross_product = glm::cross(positions[1] - positions[0], positions[2] - positions[0]);
		const float normal_length = glm::length(cross_product);
		if(normal_length <= 0.000001f) return;
		const vec3 normal = cross_product / normal_length;
		const vec2 uvs[3] = {vec2(0.0f, 0.0f), vec2(1.0f, 0.0f), vec2(0.0f, 1.0f)};
		DefaultVertex vertices[3];
		gl_point_index_t indices[3] = {0, 1, 2};
		for(uint8 index = 0; index < 3; index++)
		{
			vertices[index] = DefaultVertex(positions[index], normal, uvs[index]);
		}

		highlight_mesh.setData(
			reinterpret_cast<const float*>(vertices),
			3 * sizeof(DefaultVertex) / sizeof(float),
			indices,
			3);
		DefaultVertex::setAttributes();
		white_texture.bind();
		setDrawColor(color);
		setWorldMatrix(chunk_world_matrix);
		highlight_mesh.draw();
	}

	/**
	 * @brief Draws polygon highlights.
	 */
	void drawPolygonHighlights(const World& world)
	{
		static Mesh highlight_mesh;
		const vec4 selected_color(0.5f, 0.5f, 0.5f, 1.0f);
		const vec4 hover_color(1.0f, 0.15f, 0.15f, 1.0f);
		const LocalPlayerControllerComponent& controller = world.getLocalPlayerControllerComponent();
		for(size_t index = 0; index < controller.selected_faces.size(); index++)
		{
			const vec4& color = index < controller.selected_face_count ? selected_color : hover_color;
			drawPolygonHighlight(world, controller.selected_faces[index], highlight_mesh, color);
		}
	}
}
