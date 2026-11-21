#include <cmath>

#include <danikk_space_engine/assets/collection.h>
#include <danikk_space_engine/component/point/human/human.h>
#include <danikk_space_engine/systems/controller/controller.h>
#include <danikk_space_engine/effects/unit_selection.h>
#include <danikk_space_engine/systems/faction/faction.h>
#include <danikk_space_engine/world.h>

#include <danikk_engine/mesh.h>
#include <danikk_engine/texture_methods.h>

namespace danikk_space_engine
{
	namespace
	{
		Mesh selection_ring_mesh;
		bool selection_ring_mesh_initialized = false;

		/** @brief Creates the reusable flat annulus mesh used for unit selection. */
		void initializeSelectionRingMesh()
		{
			if(selection_ring_mesh_initialized) return;
			constexpr uint32 segment_count = 48;
			constexpr float inner_radius = 0.43f;
			constexpr float outer_radius = 0.48f;
			DynamicArray<DefaultVertex> vertices;
			DynamicArray<gl_point_index_t> indices;
			vertices.reserve(segment_count * 4);
			indices.reserve(segment_count * 6);
			for(uint32 segment = 0; segment < segment_count; ++segment)
			{
				const float angle0 = glm::two_pi<float>() * segment / segment_count;
				const float angle1 = glm::two_pi<float>() * (segment + 1) / segment_count;
				const vec3 directions[] = {
					vec3(std::cos(angle0), std::sin(angle0), 0.0f),
					vec3(std::cos(angle1), std::sin(angle1), 0.0f)};
				const uint32 first = static_cast<uint32>(vertices.size());
				vertices.pushCtor(directions[0] * inner_radius, vec3(0.0f, 0.0f, 1.0f), vec2(0.0f));
				vertices.pushCtor(directions[0] * outer_radius, vec3(0.0f, 0.0f, 1.0f), vec2(0.0f));
				vertices.pushCtor(directions[1] * outer_radius, vec3(0.0f, 0.0f, 1.0f), vec2(0.0f));
				vertices.pushCtor(directions[1] * inner_radius, vec3(0.0f, 0.0f, 1.0f), vec2(0.0f));
				const gl_point_index_t quad[] = {
					first, first + 1, first + 2,
					first, first + 2, first + 3};
				for(gl_point_index_t index : quad) indices.push(index);
			}
			selection_ring_mesh.setData(
				reinterpret_cast<const float*>(vertices.data()),
				vertices.size() * sizeof(DefaultVertex) / sizeof(float),
				indices.data(), indices.size());
			DefaultVertex::setAttributes();
			selection_ring_mesh_initialized = true;
		}
	}

	/** @copydoc drawUnitSelectionIndicators */
	void drawUnitSelectionIndicators(World& world)
	{
		const ControllerSystem& controller = world.controller;
		if(controller.selected_unit_refs.size() == 0) return;
		initializeSelectionRingMesh();
		static_asset_collection.bullet_tracer_shader.use();
		const CameraSystem& camera = world.camera;
		setViewMatrix(glm::lookAt(vec3(0.0f), camera.getFront(), camera.getUp()));
		setProjectionMatrix(glm::perspective(glm::radians(camera.vertical_fov_degrees),
			screen_ratio_gz, camera.near_clip_distance, camera.far_clip_distance));
		white_texture.bind();
		setDrawColor(vec4(1.0f));
		const glm::dvec3 camera_position = world.camera.getWorldPosition();

		for(const SelectedUnitRef& reference : controller.selected_unit_refs)
		{
			if(!world.point_data.isPointComponentSlotActive(reference.slot_id)) continue;
			const PointComponentSlot& slot = world.point_data.getPointComponentSlot(reference.slot_id);
			if(slot.generation_id != reference.generation_id
				|| slot.type_id != PointComponentTypeId::human
				|| !world.point_data.isPointActive(slot.point_id)
				|| !world.world_body_data.isWorldBodyActive(slot.world_body_id)) continue;
			const HumanComponent& human = *static_cast<const HumanComponent*>(slot.getData());
			if(human.faction_id != base_faction_ids::player || human.hasDeathTimestamp()) continue;

			const WorldBody& body = world.world_body_data.getWorldBody(slot.world_body_id);
			const glm::dvec3 center = world.getPointWorldPosition(slot.point_id);
			glm::dvec3 body_up = body.rotation * glm::dvec3(human.up);
			if(glm::length(body_up) <= 1e-9) body_up = body.rotation * glm::dvec3(human.ground_normal);
			if(glm::length(body_up) <= 1e-9) body_up = glm::dvec3(0.0, 0.0, 1.0);
			body_up = glm::normalize(body_up);
			constexpr double maximum_ground_gap = 7.0;
			const double half_height = static_cast<double>(human.getHeight()) * 0.5;
			const glm::dvec3 feet = center - body_up * half_height;
			// Query the ground for every selected unit so stale grounded state cannot leave a ring floating in the air.
			const glm::dvec3 ray_origin = center + body_up * 0.1;
			const RaycastHit ground_hit = world.raycasts.castBody(slot.world_body_id, ray_origin,
				vec3(-body_up), static_cast<float>(half_height + maximum_ground_gap + 0.25));
			if(!ground_hit.hit) continue;
			glm::dvec3 ground_normal = glm::dvec3(ground_hit.normal);
			if(glm::length(ground_normal) <= 1e-9) continue;
			ground_normal = glm::normalize(ground_normal);
			if(glm::dot(ground_normal, body_up) < 0.2) continue;
			const double ground_gap = glm::dot(feet - ground_hit.position, body_up);
			if(ground_gap < -0.25 || ground_gap > maximum_ground_gap) continue;
			const glm::dvec3 ring_position = ground_hit.position + ground_normal * 0.025;
			const glm::dvec3 up = ground_normal;

			glm::dvec3 forward = body.rotation * glm::dvec3(human.getForward());
			forward -= up * glm::dot(forward, up);
			if(glm::length(forward) <= 1e-9)
			{
				forward = glm::cross(up, glm::dvec3(1.0, 0.0, 0.0));
				if(glm::length(forward) <= 1e-9) forward = glm::cross(up, glm::dvec3(0.0, 1.0, 0.0));
			}
			forward = glm::normalize(forward);
			const glm::dvec3 right = glm::normalize(glm::cross(forward, up));
			forward = glm::normalize(glm::cross(up, right));
			mat4 orientation(1.0f);
			orientation[0] = vec4(vec3(right), 0.0f);
			orientation[1] = vec4(vec3(forward), 0.0f);
			orientation[2] = vec4(vec3(up), 0.0f);

			const mat4 world_matrix = glm::translate(mat4(1.0f), vec3(ring_position - camera_position))
				* orientation;
			setWorldMatrix(world_matrix);
			selection_ring_mesh.draw();
		}
		static_asset_collection.base3d_shader.use(true);
	}
}
