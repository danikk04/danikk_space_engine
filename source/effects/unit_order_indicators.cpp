#include <cmath>

#include <danikk_space_engine/assets/collection.h>
#include <danikk_space_engine/systems/human/human.h>
#include <danikk_space_engine/systems/controller/controller.h>
#include <danikk_space_engine/systems/unit/unit.h>
#include <danikk_space_engine/effects/unit_order_indicators.h>
#include <danikk_space_engine/world.h>

#include <danikk_engine/mesh.h>
#include <danikk_engine/texture_methods.h>

namespace danikk_space_engine
{
	namespace
	{
		/** @brief Adds one camera-facing arrow ribbon to the batched geometry. */
		void appendMovementArrow(DynamicArray<DefaultVertex>& vertices,
			DynamicArray<gl_point_index_t>& indices, const vec3& start, const vec3& end,
			const vec3& camera_up)
		{
			const vec3 offset = end - start;
			const float length = glm::length(offset);
			if(length <= 0.08f) return;
			const vec3 direction = offset / length;
			const vec3 midpoint = (start + end) * 0.5f;
			vec3 side = glm::cross(direction, -midpoint);
			if(glm::dot(side, side) <= 1e-10f) side = glm::cross(direction, camera_up);
			if(glm::dot(side, side) <= 1e-10f) return;
			side = glm::normalize(side);
			const vec3 normal = glm::normalize(glm::cross(direction, side));
			const float head_length = glm::clamp(length * 0.16f, 0.16f, 0.55f);
			const float shaft_half_width = glm::clamp(length * 0.012f, 0.018f, 0.045f);
			const float head_half_width = glm::max(shaft_half_width * 3.2f, 0.09f);
			const vec3 head_base = end - direction * head_length;
			const vec3 points[] = {
				start - side * shaft_half_width,
				start + side * shaft_half_width,
				head_base + side * shaft_half_width,
				head_base - side * shaft_half_width,
				head_base + side * head_half_width,
				end,
				head_base - side * head_half_width};
			const gl_point_index_t first = static_cast<gl_point_index_t>(vertices.size());
			for(const vec3& point : points)
				vertices.pushCtor(point, normal, vec2(0.0f));
			const gl_point_index_t local_indices[] = {0, 1, 2, 0, 2, 3, 4, 5, 6};
			for(gl_point_index_t index : local_indices) indices.push(first + index);
		}

		/** @brief Adds a camera-facing circular destination marker to the batched geometry. */
		void appendDestinationMarker(DynamicArray<DefaultVertex>& vertices,
			DynamicArray<gl_point_index_t>& indices, const vec3& center,
			const vec3& camera_right, const vec3& camera_up)
		{
			constexpr uint8 segment_count = 16;
			constexpr float radius = 0.13f;
			const vec3 normal = glm::normalize(glm::cross(camera_right, camera_up));
			const gl_point_index_t first = static_cast<gl_point_index_t>(vertices.size());
			vertices.pushCtor(center, normal, vec2(0.5f));
			for(uint8 segment = 0; segment < segment_count; ++segment)
			{
				const float angle = glm::two_pi<float>() * segment / segment_count;
				const vec3 point = center + radius * (camera_right * std::cos(angle)
					+ camera_up * std::sin(angle));
				vertices.pushCtor(point, normal, vec2(0.0f));
			}
			for(uint8 segment = 0; segment < segment_count; ++segment)
			{
				indices.push(first);
				indices.push(first + 1 + segment);
				indices.push(first + 1 + (segment + 1) % segment_count);
			}
		}
	}

	/** @copydoc drawUnitOrderIndicators */
	void drawUnitOrderIndicators(World& world)
	{
		const ControllerSystem& player = world.controller;
		if(player.selected_unit_refs.size() == 0) return;

		DynamicArray<DefaultVertex> arrow_vertices;
		DynamicArray<gl_point_index_t> arrow_indices;
		DynamicArray<DefaultVertex> marker_vertices;
		DynamicArray<gl_point_index_t> marker_indices;
		const glm::dvec3 camera_position = world.camera.getWorldPosition();
		const CameraSystem& camera = world.camera;
		const vec3 camera_up = glm::normalize(camera.getUp());
		const vec3 camera_right = glm::normalize(glm::cross(camera.getFront(), camera_up));

		for(const SelectedUnitRef& reference : player.selected_unit_refs)
		{
			const HumanData* human_data = world.humans.findHuman(reference.human_id);
			if(human_data == nullptr || human_data->generation_id != reference.generation_id
				|| !world.world_body_data.isWorldBodyActive(human_data->world_body_id)) continue;
			const HumanState& human = *human_data;
			if(human.hasDeathTimestamp()) continue;

			glm::dvec3 move_target_world;
			if(!world.unit_system.getMoveTarget(reference.human_id, reference.generation_id, move_target_world)) continue;

			const glm::dvec3 start_world = world.humans.getWorldPosition(*human_data);
			const glm::dvec3 start = start_world - camera_position;
			const glm::dvec3 target = move_target_world - camera_position;
			appendMovementArrow(arrow_vertices, arrow_indices, vec3(start), vec3(target), camera_up);
			appendDestinationMarker(marker_vertices, marker_indices, vec3(target), camera_right, camera_up);
		}
		if(arrow_indices.size() == 0 && marker_indices.size() == 0) return;

		static Mesh indicator_mesh;
		static_asset_collection.bullet_tracer_shader.use();
		setViewMatrix(glm::lookAt(vec3(0.0f), camera.getFront(), camera.getUp()));
		setProjectionMatrix(glm::perspective(glm::radians(camera.vertical_fov_degrees),
			screen_ratio_gz, camera.near_clip_distance, camera.far_clip_distance));
		white_texture.bind();
		setWorldMatrix(mat4(1.0f));
		if(arrow_indices.size() > 0)
		{
			indicator_mesh.setData(reinterpret_cast<const float*>(arrow_vertices.data()),
				arrow_vertices.size() * sizeof(DefaultVertex) / sizeof(float),
				arrow_indices.data(), arrow_indices.size());
			DefaultVertex::setAttributes();
			setDrawColor(vec4(0.15f, 0.85f, 1.0f, 1.0f));
			indicator_mesh.draw();
		}
		if(marker_indices.size() > 0)
		{
			indicator_mesh.setData(reinterpret_cast<const float*>(marker_vertices.data()),
				marker_vertices.size() * sizeof(DefaultVertex) / sizeof(float),
				marker_indices.data(), marker_indices.size());
			DefaultVertex::setAttributes();
			setDrawColor(vec4(1.0f, 0.8f, 0.15f, 1.0f));
			indicator_mesh.draw();
		}
		static_asset_collection.base3d_shader.use(true);
	}
}
