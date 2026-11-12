#include <danikk_space_engine/controller.h>
#include <danikk_space_engine/world.h>

#include <danikk_engine/input.h>
#include <danikk_engine/danikk_engine.h>
#include <danikk_engine/mesh.h>
#include <danikk_engine/texture_methods.h>
#include <danikk_framework/glm.h>

namespace danikk_space_engine
{
	vec3 LocalPlayerController::getFront() const
	{
		return vec3(
			static_cast<float>(cos(rotation.y)) * static_cast<float>(cos(rotation.x)),
			static_cast<float>(sin(rotation.y)),
			static_cast<float>(cos(rotation.y)) * static_cast<float>(sin(rotation.x)));
	}

	vec3 LocalPlayerController::getUp() const
	{
		const mat4 roll_matrix = glm::rotate(mat4(1.0f), rotation.z, getFront());
		return vec3(roll_matrix * vec4(0.0f, 1.0f, 0.0f, 0.0f));
	}

	vec3 LocalPlayerController::getRight() const
	{
		return glm::normalize(glm::cross(getFront(), getUp()));
	}

	void LocalPlayerController::tick(DynamicArray<Point>& points)
	{
		Point& target = points[target_point_id];
		const vec2 cursor_delta = getCursorDelta() * getMouseSensivity();
		const float roll_cos = cos(rotation.z);
		const float roll_sin = sin(rotation.z);
		const float yaw_delta = cursor_delta.x * roll_cos + cursor_delta.y * roll_sin;
		const float pitch_delta = -cursor_delta.x * roll_sin + cursor_delta.y * roll_cos;
		rotation.x += yaw_delta;
		rotation.y += pitch_delta;

		rotation.y = glm::clamp(rotation.y, -pi_half + 0.0001f, pi_half - 0.0001f);

		const float roll_delta = 1.5f * getTargetFrameDelay();
		if(getKeyboardState(keyboard_buttons::q))
		{
			rotation.z -= roll_delta;
		}
		if(getKeyboardState(keyboard_buttons::e))
		{
			rotation.z += roll_delta;
		}

		float speed = getTargetFrameDelay();
		if(getKeyboardState(keyboard_buttons::left_shift))
		{
			speed *= 10.0f;
		}
		if(getKeyboardState(keyboard_buttons::left_control))
		{
			speed *= 10.0f;
		}

		vec3 direction(0.0f);
		if(getKeyboardState(keyboard_buttons::w))
		{
			direction += getFront();
		}
		if(getKeyboardState(keyboard_buttons::s))
		{
			direction -= getFront();
		}
		const vec3 camera_right = getRight();
		if(getKeyboardState(keyboard_buttons::a))
		{
			direction -= camera_right;
		}
		if(getKeyboardState(keyboard_buttons::d))
		{
			direction += camera_right;
		}
		target.position += direction * speed;
	}

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
		if(topology.triangle_face_indices == nullptr)
		{
			return;
		}

		PointId face_point_ids[8]{};
		uint8 face_point_count = 0;
		for(uint8 triangle_index = 0; triangle_index < topology.triangle_count; triangle_index++)
		{
			if(topology.triangle_face_indices[triangle_index] != face_index)
			{
				continue;
			}

			const VolumeCellTriangle& triangle = topology.triangles[triangle_index];
			for(uint8 corner_index = 0; corner_index < 3; corner_index++)
			{
				const PointId point_id = cell.points[triangle.points[corner_index]];
				if(point_id >= world.points.size())
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
		if(face_point_count < 3 || world.controller.target_point_id >= world.points.size())
		{
			return;
		}

		vec3 face_center(0.0f);
		for(uint8 point_index = 0; point_index < face_point_count; point_index++)
		{
			face_center += world.points[face_point_ids[point_index]].position;
		}
		face_center /= static_cast<float>(face_point_count);

		const mat4 region_world_matrix = region.createWorldMatrix();
		const vec3 face_center_world = vec3(region_world_matrix * vec4(face_center, 1.0f));
		const vec3 camera_position = world.points[world.controller.target_point_id].position;
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
		setWorldMatrix(region_world_matrix);
		highlight_mesh.draw();
	}

	void LocalPlayerController::frame(const World& world)
	{
		const RaycastHit hit = world.raycastFromCamera();
		if(getMouseState(mouse_buttons::left) == button_states::press)
		{
			if(hit.hit)
			{
				size_t selected_index = selected_polygons.size();
				for(size_t i = 0; i < selected_polygons.size(); i++)
				{
					const SelectedPolygon& selected = selected_polygons[i];
					if(selected.region_index == hit.region_index
						&& selected.cell_index == hit.cell_index
						&& selected.face_index == hit.face_index)
					{
						selected_index = i;
						break;
					}
				}

				if(selected_index < selected_polygons.size())
				{
					for(size_t i = selected_index; i + 1 < selected_polygons.size(); i++)
					{
						selected_polygons[i] = selected_polygons[i + 1];
					}
					selected_polygons.resize(selected_polygons.size() - 1);
					if(selected_polygons.size() == 0)
					{
						selected_polygons = DynamicArray<SelectedPolygon>();
					}
				}
				else
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
