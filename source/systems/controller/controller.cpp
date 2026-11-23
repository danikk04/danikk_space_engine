#include <danikk_space_engine/systems/controller/controller.h>
#include <danikk_space_engine/systems/planet/system.h>
#include <danikk_space_engine/systems/world_body/system.h>
#include <danikk_space_engine/world.h>
#include <danikk_space_engine/config.h>
#include <danikk_space_engine/assets/human_skin.h>
#include <danikk_space_engine/systems/human/human.h>
#include <danikk_space_engine/effects/world_body_preview.h>
#include <danikk_space_engine/effects/event_spawn_preview.h>
#include <danikk_space_engine/assets/collection.h>
#include <danikk_space_engine/systems/telemetry/telemetry.h>
#include <danikk_engine/texture_methods.h>
#include <danikk_framework/log.h>

#include <danikk_space_engine/systems/world_body/world_body.h>
#include <danikk_space_engine/systems/camera/camera.h>
#include <danikk_space_engine/systems/raycast/raycast.h>

namespace danikk_space_engine
{
	namespace
	{
		void drawHumanPlacementEffect(World& world, const RaycastHit& hit, HumanSkinId skin_id)
		{
			if(!hit.hit || !world.getWorldBodySystem().isWorldBodyActive(hit.body_id)) return;
			const WorldBody& body = world.getWorldBodySystem().getWorldBody(hit.body_id);
			const glm::dquat inverse_rotation = glm::conjugate(body.rotation);
			const glm::dvec3 local_position = inverse_rotation * (hit.position - body.position);
			vec3 up = vec3(inverse_rotation * glm::dvec3(hit.normal));
			if(world.getPlanetSystem().isPlanetWorldBody(hit.body_id))
			{
				if(glm::length(local_position) <= 1e-6) return;
				up = vec3(glm::normalize(local_position));
			}
			if(glm::length(up) <= 1e-6f) return;
			up = glm::normalize(up);
			const vec3 local_normal = vec3(inverse_rotation * glm::dvec3(hit.normal));
			if(glm::dot(up, local_normal) < 0.5f) return;
			const vec3 forward = vec3(inverse_rotation
				* (world.getCameraSystem().getWorldPosition() - hit.position));

			HumanState preview_human;
			preview_human.up = up;
			preview_human.setInitialForward(forward);
			preview_human.appearance.skin_id = skin_id;
			const float height = preview_human.getHeight();
			const glm::dvec3 feet_position = local_position + glm::dvec3(up * 0.02f);
			const glm::dvec3 center_world = body.position
				+ body.rotation * (feet_position + glm::dvec3(up * (height * 0.5f)));
			const glm::dvec3 world_up = body.rotation * glm::dvec3(up);
			glm::dvec3 world_forward = body.rotation * glm::dvec3(preview_human.getForward());
			world_forward -= world_up * glm::dot(world_forward, world_up);
			if(glm::length(world_forward) <= 1e-6) return;
			world_forward = glm::normalize(world_forward);
			const glm::dvec3 world_right = glm::normalize(glm::cross(world_forward, world_up));
			mat4 orientation(1.0f);
			orientation[0] = glm::vec4(world_right, 0.0);
			orientation[1] = glm::vec4(world_forward, 0.0);
			orientation[2] = glm::vec4(world_up, 0.0);
			const mat4 body_matrix = glm::translate(mat4(1.0f),
				vec3(center_world - world.getCameraSystem().getWorldPosition())) * orientation;
			const mat4 feet_matrix = body_matrix
				* glm::translate(mat4(1.0f), vec3(0.0f, 0.0f, -0.5f))
				* glm::scale(mat4(1.0f), vec3(height));
			drawHumanAppearance(preview_human, feet_matrix, vec4(0.08f, 0.35f, 1.0f, 0.55f));
		}
	}

	void ControllerSystem::init()
	{
		danikk_framework::formatLogInfo("Init ControllerSystem");
		is_bound = false;
	}
	void ControllerSystem::shutdown()
	{
		danikk_framework::formatLogInfo("Shutdown ControllerSystem");
		unbind();
	}
	/** @brief Resets controller state and enables control of the active camera. */
	void ControllerSystem::reset()
	{
		is_bound = true;
		mode = PlayerMode::god;
		active_tool_id = no_tool_catalog_entry;
		selected_hotbar_slot = 0;
		for(ToolCatalogEntryId& entry_id : hotbar_entry_ids) entry_id = 0;
		tool_catalog_open = false;
		build_part_size = vec3(1.0f);
		build_part_size_axis = 2;
		build_part_size_scroll_accumulator = 0.0f;
		build_part_rotation_step = 0;
		build_part_rotation_scroll_accumulator = 0.0f;
		selected_unit_refs.clear();
		camera_movement_speed = 1.0f;
		dragged_world_body_id = static_cast<WorldBodyId>(-1);
		dragged_body_distance = 0.0;
		dragged_hit_offset = glm::dvec3(0.0);
		dragged_human_id = static_cast<HumanId>(-1);
		dragged_human_distance = 0.0;
		dragged_human_followed_ground = true;
	}

	/** @brief Returns whether local control is enabled and the camera remains active. */
	bool ControllerSystem::controls() const
	{
		return is_bound && world.getCameraSystem().isActive();
	}

	/** @brief Releases world ownership and transient selections before the world is destroyed. */
	void ControllerSystem::unbind()
	{
		if(!is_bound) return;
		is_bound = false;
		selected_unit_refs.clear();
		dragged_world_body_id = static_cast<WorldBodyId>(-1);
		dragged_human_id = static_cast<HumanId>(-1);
	}

	void drawCubeToolEffect(World& world, const ControllerSystem& controller, const RaycastHit& hit)
	{
		drawWorldBodyPlacementPreview(world, hit, BuildPartShape::cube,
			controller.build_part_size, controller.build_part_size_axis,
			controller.build_part_rotation_step);
	}

	void drawSlopeToolEffect(World& world, const ControllerSystem& controller, const RaycastHit& hit)
	{
		drawWorldBodyPlacementPreview(world, hit, BuildPartShape::slope,
			controller.build_part_size, controller.build_part_size_axis,
			controller.build_part_rotation_step);
	}

	void drawHumanToolEffect(World& world, const ControllerSystem&, const RaycastHit& hit)
	{
		drawHumanPlacementEffect(world, hit, human_skin_ids::usmc_marpat_woodland);
	}

	void drawReimuToolEffect(World& world, const ControllerSystem&, const RaycastHit& hit)
	{
		drawHumanPlacementEffect(world, hit, human_skin_ids::reimu_hakurei);
	}

	void drawMarisaToolEffect(World& world, const ControllerSystem&, const RaycastHit& hit)
	{
		drawHumanPlacementEffect(world, hit, human_skin_ids::marisa_kirisame);
	}

	void drawTsumiBukuroToolEffect(World& world, const ControllerSystem&, const RaycastHit& hit)
	{
		drawHumanPlacementEffect(world, hit, human_skin_ids::tsumi_bukuro);
	}

	void drawAlliesToolEffect(World& world, const ControllerSystem&, const RaycastHit& hit)
	{
		drawEventSpawnPreview(world, hit, base_faction_ids::player);
	}

	void drawPiratesToolEffect(World& world, const ControllerSystem&, const RaycastHit& hit)
	{
		drawEventSpawnPreview(world, hit, base_faction_ids::pirates);
	}

	/** @copydoc ControllerSystem::renderPreview */
	void ControllerSystem::renderPreview() const
	{
		DSE_WATCH();
		if(!settings.show_world_body_preview) return;
		const ToolEffectHandler effect = getActiveTool().effect;
		if(effect == nullptr) return;

		static_asset_collection.base3d_shader.use(true);
		setViewMatrix(glm::lookAt(vec3(0.0f), world.getCameraSystem().getFront(), world.getCameraSystem().getUp()));
		setProjectionMatrix(glm::perspective(glm::radians(world.getCameraSystem().vertical_fov_degrees),
			screen_ratio_gz, world.getCameraSystem().near_clip_distance, world.getCameraSystem().far_clip_distance));
		const RaycastHit hit = world.getRaycastSystem().castFromCamera();
		effect(world, *this, hit);
	}
}
