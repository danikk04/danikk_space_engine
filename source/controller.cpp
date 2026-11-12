#include <danikk_space_engine/controller.h>
#include <danikk_space_engine/world.h>

#include <danikk_engine/texture_methods.h>

namespace danikk_space_engine
{
	void LocalPlayerController::frame(const World& world)
	{
		const RaycastHit hit = world.raycastFromCamera();
		if(!hit.hit || hit.region_index >= world.regions.size())
		{
			return;
		}

		const Region& region = world.regions[hit.region_index];
		if(hit.cell_index >= region.volume_cells.size())
		{
			return;
		}

		const VolumeCell& cell = region.volume_cells[hit.cell_index];
		const VolumeCellTopologyDefinition& topology = getVolumeCellTopologyDefinition(cell.topology);
		if(hit.triangle_index >= topology.triangle_count)
		{
			return;
		}

		const VolumeCellTriangle& triangle = topology.triangles[hit.triangle_index];
		const vec3 a = world.points[cell.points[triangle.points[0]]].position;
		const vec3 b = world.points[cell.points[triangle.points[1]]].position;
		const vec3 c = world.points[cell.points[triangle.points[2]]].position;
		const vec3 normal = glm::normalize(glm::cross(b - a, c - a));
		constexpr float highlight_offset = 0.002f;

		DefaultVertex vertices[3]
		{
			DefaultVertex(a + normal * highlight_offset, normal, vec2(0.0f, 0.0f)),
			DefaultVertex(b + normal * highlight_offset, normal, vec2(1.0f, 0.0f)),
			DefaultVertex(c + normal * highlight_offset, normal, vec2(0.0f, 1.0f))
		};
		const gl_point_index_t indices[3] = {0, 1, 2};
		static Mesh highlight_mesh;
		highlight_mesh.setData(
			reinterpret_cast<const float*>(vertices),
			3 * sizeof(DefaultVertex) / sizeof(float),
			indices,
			3);
		DefaultVertex::setAttributes();

		white_texture.bind();
		setDrawColor(vec4(0.7f, 0.35f, 0.05f, 1.0f));
		setWorldMatrix(region.createWorldMatrix());
		highlight_mesh.draw();
	}
}
