#include <danikk_space_engine/effects/world_body_preview.h>

#include <danikk_space_engine/world.h>
#include <danikk_space_engine/tool.h>
#include <danikk_space_engine/geometry/cube.h>
#include <danikk_space_engine/geometry/build_part.h>
#include <danikk_space_engine/assets/collection.h>
#include <danikk_engine/texture_methods.h>

namespace danikk_space_engine
{
	/**
	 * @brief Creates transient sized BuildPart geometry at the proposed placement and draws it blue.
	 * @param world World supplying camera position and placement transforms.
	 * @param hit Surface hit that determines cube center and outward orientation.
	 * @param shape BuildPart primitive selected for placement.
	 * @param size Local dimensions in metres applied to the primitive.
	 * @param axis Selected local size axis used to color the corresponding faces.
	 * A miss produces no draw. Dimensions are applied in local shape space; its center is placed
	 * half its local Z size plus 0.03 m above the hit. Geometry is converted to
	 * camera-relative coordinates; this function only updates a static render mesh.
	 */
	void drawWorldBodyPlacementPreview(
		World& world, const RaycastHit& hit, BuildPartShape shape, const vec3& size,
		uint8 axis, uint8 rotation_step)
	{
		if(!hit.hit) return;

		const BuildPartPlacement placement = getBuildPartPlacement(world, hit, shape, size, rotation_step);
		const vec3 camera_relative_center = vec3(placement.world_position - world.getCameraWorldPosition());
		const BuildPartGeometry& geometry = getBuildPartGeometry(shape);

		DynamicArray<DefaultVertex> vertices;
		DynamicArray<gl_point_index_t> indices;
		DynamicArray<DefaultVertex> axis_vertices;
		DynamicArray<gl_point_index_t> axis_indices;
		vertices.reserve(geometry.triangles.size() * 3);
		indices.reserve(geometry.triangles.size() * 3);
		for(const CubeTriangle& triangle : geometry.triangles)
		{
			const vec3 local_a = geometry.positions[triangle.points[0]] * size;
			const vec3 local_b = geometry.positions[triangle.points[1]] * size;
			const vec3 local_c = geometry.positions[triangle.points[2]] * size;
			const vec3 local_normal = glm::abs(glm::cross(local_b - local_a, local_c - local_a));
			const bool axis_face = axis < 3 && local_normal[axis] > 0.000001f
				&& local_normal[axis] >= local_normal[(axis + 1) % 3]
				&& local_normal[axis] >= local_normal[(axis + 2) % 3];
			DynamicArray<DefaultVertex>& target_vertices = axis_face ? axis_vertices : vertices;
			DynamicArray<gl_point_index_t>& target_indices = axis_face ? axis_indices : indices;
			const vec3 a = vec3(placement.world_rotation * vec4(geometry.positions[triangle.points[0]] * size, 0.0f));
			const vec3 b = vec3(placement.world_rotation * vec4(geometry.positions[triangle.points[1]] * size, 0.0f));
			const vec3 c = vec3(placement.world_rotation * vec4(geometry.positions[triangle.points[2]] * size, 0.0f));
			const vec3 cross_product = glm::cross(b - a, c - a);
			const float cross_length = glm::length(cross_product);
			if(cross_length <= 0.000001f) continue;
			const vec3 normal = cross_product / cross_length;
			const vec3 triangle_points[3] = {a, b, c};
			for(uint8 corner = 0; corner < 3; corner++)
			{
				target_indices.push(static_cast<gl_point_index_t>(target_vertices.size()));
				target_vertices.pushCtor(DefaultVertex(camera_relative_center + triangle_points[corner], normal, vec2(0.0f)));
			}
		}
		static Mesh preview_mesh;
		white_texture.bind();
		setWorldMatrix(mat4(1.0f));
		auto drawFaces = [&](const DynamicArray<DefaultVertex>& face_vertices,
			const DynamicArray<gl_point_index_t>& face_indices, const vec4& color)
		{
			if(face_vertices.size() == 0) return;
			preview_mesh.setData(reinterpret_cast<const float*>(face_vertices.data()),
				face_vertices.size() * sizeof(DefaultVertex) / sizeof(float),
				face_indices.data(), face_indices.size());
			DefaultVertex::setAttributes();
			setDrawColor(color);
			preview_mesh.draw();
		};
		drawFaces(vertices, indices, vec4(0.08f, 0.35f, 1.0f, 0.55f));
		const vec4 axis_colors[] = {
			vec4(1.0f, 0.15f, 0.15f, 0.8f), vec4(0.15f, 1.0f, 0.15f, 0.8f),
			vec4(0.3f, 0.7f, 1.0f, 0.8f)};
		if(axis < 3) drawFaces(axis_vertices, axis_indices, axis_colors[axis]);
	}
}
