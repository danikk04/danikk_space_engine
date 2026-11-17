#include <danikk_space_engine/effects/world_body_preview.h>

#include <danikk_space_engine/world/world.h>
#include <danikk_space_engine/world/cube_mesh.h>
#include <danikk_space_engine/world/build_part_shape.h>
#include <danikk_space_engine/asset.h>
#include <danikk_engine/texture_methods.h>

namespace danikk_space_engine
{
	/**
	 * @brief Creates transient sized BuildPart geometry at the proposed placement and draws it blue.
	 * @param world World supplying camera position and placement transforms.
	 * @param hit Surface hit that determines cube center and outward orientation.
	 * @param shape BuildPart primitive selected for placement.
	 * @param size Local dimensions in metres applied to the primitive.
	 * A miss produces no draw. Dimensions are applied in local shape space; its center is placed
	 * half its local Z size plus 0.03 m above the hit. Geometry is converted to
	 * camera-relative coordinates; this function only updates a static render mesh.
	 */
	void drawWorldBodyPlacementPreview(
		World& world, const RaycastHit& hit, BuildPartShape shape, const vec3& size)
	{
		if(!hit.hit) return;

		constexpr double surface_offset = 0.03;
		vec3 up = hit.normal;
		const float normal_length = glm::length(up);
		if(normal_length <= 0.0001f) up = vec3(0.0f, 0.0f, 1.0f);
		else up /= normal_length;

		const glm::dvec3 part_center = hit.position
			+ glm::dvec3(up) * (static_cast<double>(size.z) * 0.5 + surface_offset);
		const mat4 part_rotation = createBuildPartRotation(up);
		const vec3 camera_relative_center = vec3(part_center - world.getCameraWorldPosition());
		const BuildPartGeometry& geometry = getBuildPartGeometry(shape);

		DynamicArray<DefaultVertex> vertices;
		DynamicArray<gl_point_index_t> indices;
		vertices.reserve(geometry.triangles.size() * 3);
		indices.reserve(geometry.triangles.size() * 3);
		for(const CubeTriangle& triangle : geometry.triangles)
		{
			const vec3 a = vec3(part_rotation * vec4(geometry.positions[triangle.points[0]] * size, 0.0f));
			const vec3 b = vec3(part_rotation * vec4(geometry.positions[triangle.points[1]] * size, 0.0f));
			const vec3 c = vec3(part_rotation * vec4(geometry.positions[triangle.points[2]] * size, 0.0f));
			const vec3 cross_product = glm::cross(b - a, c - a);
			const float cross_length = glm::length(cross_product);
			if(cross_length <= 0.000001f) continue;
			const vec3 normal = cross_product / cross_length;
			const vec3 triangle_points[3] = {a, b, c};
			for(uint8 corner = 0; corner < 3; corner++)
			{
				indices.push(static_cast<gl_point_index_t>(vertices.size()));
				vertices.pushCtor(DefaultVertex(camera_relative_center + triangle_points[corner], normal, vec2(0.0f)));
			}
		}
		if(vertices.size() == 0) return;

		static Mesh preview_mesh;
		preview_mesh.setData(
			reinterpret_cast<const float*>(vertices.data()),
			vertices.size() * sizeof(DefaultVertex) / sizeof(float),
			indices.data(),
			indices.size());
		DefaultVertex::setAttributes();
		white_texture.bind();
		setDrawColor(vec4(0.08f, 0.35f, 1.0f, 0.55f));
		setWorldMatrix(mat4(1.0f));
		preview_mesh.draw();
	}
}
