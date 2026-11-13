#include <danikk_space_engine/polygon_highlight.h>

#include <danikk_space_engine/world/world.h>

#include <danikk_engine/texture_methods.h>

namespace danikk_space_engine
{
	static bool isSelected(const DynamicArray<FacePtr>& selected_polygons, const FacePtr& face)
	{
		for(const FacePtr& selected : selected_polygons)
		{
			if(selected == face)
			{
				return true;
			}
		}
		return false;
	}

	// Draws the selected face of a volume cell as a slightly inset overlay toward the camera.
	// Collects unique face points from the topology triangles, computes the face center,
	// rebuilds its triangles with a small camera offset, uploads them, and draws in the chunk transform.
	void drawPolygonHighlight(
		const World& world,
		const FacePtr& face,
		Mesh& highlight_mesh,
		const vec4& color)
	{
		if(face.chunk_id >= world.data.getChunkCount())
		{
			return;
		}

		const Chunk& chunk = world.data.getChunk(face.chunk_id);
		if(face.cell_id >= world.data.getVolumeCellCount())
		{
			return;
		}

		const VolumeCell& cell = world.data.getVolumeCell(face.cell_id);
		const VolumeCellTopologyDefinition& topology = getVolumeCellTopologyDefinition(cell.topology);
		if(topology.triangle_face_indices == nullptr)
		{
			return;
		}

		PointId face_point_ids[8]{};
		uint8 face_point_count = 0;
		for(uint8 triangle_index = 0; triangle_index < topology.triangle_count; triangle_index++)
		{
			if(topology.triangle_face_indices[triangle_index] != face.face_index)
			{
				continue;
			}

			const VolumeCellTriangle& triangle = topology.triangles[triangle_index];
			for(uint8 corner_index = 0; corner_index < 3; corner_index++)
			{
				const PointId point_id = cell.points[triangle.points[corner_index]];
				if(point_id >= world.data.getPointCount())
				{
					continue;
				}

				bool already_added = false;
				for(uint8 point_index = 0; point_index < face_point_count; point_index++)
				{
					if(face_point_ids[point_index] == point_id)
					{
						already_added = true;
						break;
					}
				}
				if(!already_added && face_point_count < 8)
				{
					face_point_ids[face_point_count++] = point_id;
				}
			}
		}
		if(face_point_count < 3 || world.getCameraSlot().point_id >= world.data.getPointCount())
		{
			return;
		}

		vec3 face_center(0.0f);
		for(uint8 point_index = 0; point_index < face_point_count; point_index++)
		{
			face_center += world.data.getPoint(face_point_ids[point_index]).position;
		}
		face_center /= static_cast<float>(face_point_count);

		const mat4 chunk_world_matrix = chunk.createWorldMatrix();
		const vec3 face_center_world = vec3(chunk_world_matrix * vec4(face_center, 1.0f));
		const vec3 camera_position = world.getCameraPoint().position;
		const vec3 camera_delta = camera_position - face_center_world;
		const float camera_distance = glm::length(camera_delta);
		const vec3 camera_offset_direction = camera_distance > 0.000001f
			? camera_delta / camera_distance
			: vec3(0.0f);
		constexpr float highlight_offset = 0.002f;
		constexpr float highlight_shrink_factor = 1.05f;
		DefaultVertex vertices[36];
		gl_point_index_t indices[36];
		uint8 vertex_count = 0;

		for(uint8 triangle_index = 0; triangle_index < topology.triangle_count; triangle_index++)
		{
			if(topology.triangle_face_indices[triangle_index] != face.face_index)
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
			if(point_ids[0] >= world.data.getPointCount() || point_ids[1] >= world.data.getPointCount() || point_ids[2] >= world.data.getPointCount())
			{
				continue;
			}

			const vec3 a = world.data.getPoint(point_ids[0]).position;
			const vec3 b = world.data.getPoint(point_ids[1]).position;
			const vec3 c = world.data.getPoint(point_ids[2]).position;
			const vec3 shrunk_a = face_center + (a - face_center) / highlight_shrink_factor + camera_offset_direction * highlight_offset;
			const vec3 shrunk_b = face_center + (b - face_center) / highlight_shrink_factor + camera_offset_direction * highlight_offset;
			const vec3 shrunk_c = face_center + (c - face_center) / highlight_shrink_factor + camera_offset_direction * highlight_offset;
			const vec3 cross_product = glm::cross(shrunk_b - shrunk_a, shrunk_c - shrunk_a);
			const float normal_length = glm::length(cross_product);
			if(normal_length <= 0.000001f)
			{
				continue;
			}
			const vec3 normal = cross_product / normal_length;
			const vec3 positions[3] = {shrunk_a, shrunk_b, shrunk_c};
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
		setWorldMatrix(chunk_world_matrix);
		highlight_mesh.draw();
	}

	void drawPolygonHighlights(const World& world)
	{
		static Mesh highlight_mesh;
		const vec4 selected_color(0.5f, 0.5f, 0.5f, 1.0f);
		const vec4 hover_color(1.0f, 0.15f, 0.15f, 1.0f);

		for(const FacePtr& selected : world.getLocalPlayerController().selected_polygons)
		{
			drawPolygonHighlight(world, selected, highlight_mesh, selected_color);
		}

		const RaycastHit hit = world.raycastFromCamera();
		if(hit.hit && !isSelected(world.getLocalPlayerController().selected_polygons, hit.face))
		{
			drawPolygonHighlight(world, hit.face, highlight_mesh, hover_color);
		}
	}
}
