#include <danikk_space_engine/effects/world_body_preview.h>

#include <danikk_space_engine/world/world.h>
#include <danikk_space_engine/world/cube_mesh.h>
#include <danikk_space_engine/asset.h>
#include <danikk_engine/texture_methods.h>

namespace danikk_space_engine
{
	// Builds a blue cube at the same position used by Build -> Create WorldBody.
	// Geometry is camera-relative to retain precision while previewing far from the origin.
	void drawWorldBodyPlacementPreview(World& world, const RaycastHit& hit)
	{
		if(!hit.hit) return;

		constexpr float cube_size = 1.0f;
		constexpr double surface_offset = 0.03;
		vec3 up = hit.normal;
		const float normal_length = glm::length(up);
		if(normal_length <= 0.0001f) up = vec3(0.0f, 0.0f, 1.0f);
		else up /= normal_length;

		const glm::dvec3 cube_center = hit.position
			+ glm::dvec3(up) * (static_cast<double>(cube_size) * 0.5 + surface_offset);
		const vec3 camera_relative_center = vec3(cube_center - world.getCameraWorldPosition());
		DynamicArray<vec3> cube_positions;
		DynamicArray<CubeTriangle> cube_triangles;
		generateCubeGeometry(cube_size, cube_positions, cube_triangles);

		DefaultVertex vertices[36];
		gl_point_index_t indices[36];
		size_t vertex_count = 0;
		for(const CubeTriangle& triangle : cube_triangles)
		{
			const vec3 a = cube_positions[triangle.points[0]];
			const vec3 b = cube_positions[triangle.points[1]];
			const vec3 c = cube_positions[triangle.points[2]];
			const vec3 cross_product = glm::cross(b - a, c - a);
			const float cross_length = glm::length(cross_product);
			if(cross_length <= 0.000001f) continue;
			const vec3 normal = cross_product / cross_length;
			const vec3 triangle_points[3] = {a, b, c};
			for(uint8 corner = 0; corner < 3; corner++)
			{
				vertices[vertex_count] = DefaultVertex(
					camera_relative_center + triangle_points[corner], normal, vec2(0.0f));
				indices[vertex_count] = vertex_count;
				vertex_count++;
			}
		}
		if(vertex_count == 0) return;

		static Mesh preview_mesh;
		preview_mesh.setData(
			reinterpret_cast<const float*>(vertices),
			vertex_count * sizeof(DefaultVertex) / sizeof(float),
			indices,
			vertex_count);
		DefaultVertex::setAttributes();
		white_texture.bind();
		setDrawColor(vec4(0.08f, 0.35f, 1.0f, 0.55f));
		setWorldMatrix(mat4(1.0f));
		preview_mesh.draw();
	}
}
