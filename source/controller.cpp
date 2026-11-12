#include <danikk_space_engine/controller.h>
#include <danikk_space_engine/world.h>

#include <danikk_engine/texture_methods.h>

namespace danikk_space_engine
{
	static bool isSelected(
		const DynamicArray<LocalPlayerController::SelectedPolygon>& selected_polygons,
		uint32 region_index,
		uint32 cell_index,
		uint8 face_index)
	{
		for(const LocalPlayerController::SelectedPolygon& selected : selected_polygons)
		{
			if(selected.region_index == region_index
				&& selected.cell_index == cell_index
				&& selected.face_index == face_index)
			{
				return true;
			}
		}
		return false;
	}

	static void drawPolygonHighlight(
		const World& world,
		uint32 region_index,
		uint32 cell_index,
		uint8 face_index,
		Mesh& highlight_mesh,
		const vec4& color)
	{
		if(region_index >= world.regions.size())
		{
			return;
		}

		const Region& region = world.regions[region_index];
		if(cell_index >= region.volume_cells.size())
		{
			return;
		}

		const VolumeCell& cell = region.volume_cells[cell_index];
		const VolumeCellTopologyDefinition& topology = getVolumeCellTopologyDefinition(cell.topology);
		DefaultVertex vertices[36];
		gl_point_index_t indices[36];
		uint8 vertex_count = 0;
		constexpr float highlight_offset = 0.002f;

		for(uint8 triangle_index = 0; triangle_index < topology.triangle_count; triangle_index++)
		{
			if(topology.triangle_face_indices[triangle_index] != face_index)
			{
				continue;
			}

			const VolumeCellTriangle& triangle = topology.triangles[triangle_index];
			const PointId point_ids[3]
			{
				cell.points[triangle.points[0]],
				cell.points[triangle.points[1]],
				cell.points[triangle.points[2]]
			};
			if(point_ids[0] >= world.points.size() || point_ids[1] >= world.points.size() || point_ids[2] >= world.points.size())
			{
				continue;
			}

			const vec3 a = world.points[point_ids[0]].position;
			const vec3 b = world.points[point_ids[1]].position;
			const vec3 c = world.points[point_ids[2]].position;
			const vec3 cross_product = glm::cross(b - a, c - a);
			const float normal_length = glm::length(cross_product);
			if(normal_length <= 0.000001f)
			{
				continue;
			}
			const vec3 normal = cross_product / normal_length;
			const vec3 positions[3]
			{
				a + normal * highlight_offset,
				b + normal * highlight_offset,
				c + normal * highlight_offset
			};
			const vec2 uvs[3] = {vec2(0.0f, 0.0f), vec2(1.0f, 0.0f), vec2(0.0f, 1.0f)};

			for(uint8 vertex_index = 0; vertex_index < 3; vertex_index++)
			{
				vertices[vertex_count] = DefaultVertex(positions[vertex_index], normal, uvs[vertex_index]);
				indices[vertex_count] = vertex_count;
				vertex_count++;
			}
		}

		if(vertex_count == 0)
		{
			return;
		}

		highlight_mesh.setData(
			reinterpret_cast<const float*>(vertices),
			vertex_count * sizeof(DefaultVertex) / sizeof(float),
			indices,
			vertex_count);
		DefaultVertex::setAttributes();
		white_texture.bind();
		setDrawColor(color);
		setWorldMatrix(region.createWorldMatrix());
		highlight_mesh.draw();
	}

	void LocalPlayerController::frame(const World& world)
	{
		const RaycastHit hit = world.raycastFromCamera();
		if(getMouseState(mouse_buttons::left) == button_states::press)
		{
			if(hit.hit)
			{
				if(!isSelected(selected_polygons, hit.region_index, hit.cell_index, hit.face_index))
				{
					selected_polygons.pushCtor(hit.region_index, hit.cell_index, hit.face_index);
				}
			}
			else
			{
				selected_polygons = DynamicArray<SelectedPolygon>();
			}
		}

		static Mesh highlight_mesh;
		const vec4 selected_color(0.5f, 0.5f, 0.5f, 1.0f);
		const vec4 hover_color(1.0f, 0.15f, 0.15f, 1.0f);

		for(const SelectedPolygon& selected : selected_polygons)
		{
			drawPolygonHighlight(
				world,
				selected.region_index,
				selected.cell_index,
				selected.face_index,
				highlight_mesh,
				selected_color);
		}

		if(hit.hit && !isSelected(selected_polygons, hit.region_index, hit.cell_index, hit.face_index))
		{
			drawPolygonHighlight(
				world,
				hit.region_index,
				hit.cell_index,
				hit.face_index,
				highlight_mesh,
				hover_color);
		}
	}
}
