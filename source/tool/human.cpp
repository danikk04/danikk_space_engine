#include <cmath>

#include <danikk_engine/input.h>
#include <danikk_space_engine/tool.h>
#include <danikk_space_engine/world.h>
#include <danikk_space_engine/systems/human/human.h>
#include <danikk_space_engine/assets/human_skin.h>
#include <danikk_space_engine/systems/controller/controller.h>
#include <danikk_space_engine/systems/faction/faction.h>
#include <danikk_space_engine/systems/planet/system.h>
#include <danikk_space_engine/math/ray.h>

namespace danikk_space_engine
{
	namespace
	{
		constexpr HumanId no_human = static_cast<HumanId>(-1);

		/**
		 * @brief Restores the captured human's ground-follow setting and clears drag state.
		 * @param world World used to resolve the captured component slot.
		 * @param controller Controller state whose human capture fields are reset.
		 * If the captured slot is still active and still represents a human, its
		 * saved follow_ground value is restored before the IDs and distance are reset.
		 */
		void clearHumanDrag(World& world, ControllerSystem& controller)
		{
			if(HumanData* human = world.humans.findHuman(controller.dragged_human_id))
				human->follow_ground = controller.dragged_human_followed_ground;
			controller.dragged_human_id = no_human;
			controller.dragged_human_distance = 0.0;
			controller.dragged_human_followed_ground = true;
		}

		/**
		 * @brief Creates an NPC slightly above a valid surface hit with a body-relative up axis.
		 * @param world World receiving the NPC and its component slots.
		 * @param hit Raycast result used to find the owning body and spawn location.
		 * Planet up is radial from the body center; other bodies use the hit normal.
		 * Hits with an invalid normal or a slope facing away from the chosen up axis
		 * are rejected. The hit must satisfy dot(up, local_normal) >= 0.5; the
		 * human's feet are placed 0.02 m above the hit before createNpc() offsets
		 * its center by half the configured height.
		 */
		void createHumanAtHit(World& world, const RaycastHit& hit,
			HumanSkinId skin_id, FactionId faction_id)
		{
			if(!world.world_body_data.isWorldBodyActive(hit.body_id)) return;
			const WorldBodyId body_id = hit.body_id;
			const WorldBody& body = world.world_body_data.getWorldBody(body_id);
			const glm::dquat inverse_rotation = glm::conjugate(body.rotation);
			const glm::dvec3 local_position = inverse_rotation * (hit.position - body.position);
			vec3 up = vec3(inverse_rotation * glm::dvec3(hit.normal));
			if(world.planets.isPlanet(body.planet_id))
			{
				if(glm::length(local_position) < 1e-6) return;
				up = vec3(glm::normalize(local_position));
			}
			if(glm::length(up) < 1e-6f) return;
			up = glm::normalize(up);
			const vec3 local_normal = vec3(inverse_rotation * glm::dvec3(hit.normal));
			if(glm::dot(up, local_normal) < 0.5f) return;
			const vec3 forward = vec3(inverse_rotation * (world.camera.getWorldPosition() - hit.position));
			const HumanId human_id = world.humans.createNpc(body_id,
				local_position + glm::dvec3(up * 0.02f), up, forward, faction_id);
			if(HumanData* human = world.humans.findHuman(human_id)) human->appearance.skin_id = skin_id;
		}
	}

	ToolApplyResult applyCreateHumanTool(World& world, const RaycastHit& hit, const vec3&, uint8)
	{
		if(hit.hit) createHumanAtHit(world, hit, human_skin_ids::usmc_marpat_woodland,
			base_faction_ids::player);
		return ToolApplyResult();
	}

	ToolApplyResult applyCreateReimuTool(World& world, const RaycastHit& hit, const vec3&, uint8)
	{
		if(hit.hit) createHumanAtHit(world, hit, human_skin_ids::reimu_hakurei,
			base_faction_ids::player);
		return ToolApplyResult();
	}

	ToolApplyResult applyCreateMarisaTool(World& world, const RaycastHit& hit, const vec3&, uint8)
	{
		if(hit.hit) createHumanAtHit(world, hit, human_skin_ids::marisa_kirisame,
			base_faction_ids::player);
		return ToolApplyResult();
	}

	ToolApplyResult applyCreateTsumiBukuroTool(World& world, const RaycastHit& hit,
		const vec3&, uint8)
	{
		if(hit.hit) createHumanAtHit(world, hit, human_skin_ids::tsumi_bukuro,
			base_faction_ids::pirates);
		return ToolApplyResult();
	}

	/**
	 * @brief Captures the nearest ray-hit human capsule and moves its point in front of the camera.
	 * @param world World used for ray tests, transforms, and resource access.
	 * @param controller Local controller storing the dragged human IDs and distance.
	 * @param hit Nearest surface raycast hit, used to prevent selecting a human through geometry.
	 * @param enabled Whether the unified Drag tool should process a human capture.
	 * @param mouse_state Current left-button state; release or disable restores ground following.
	 * @param scroll_delta Wheel movement changes distance only while Alt is held.
	 * @param alt_pressed Enables wheel distance adjustment.
	 * On press, capsule intersections are considered only when closer than the
	 * surface hit. While captured, ground-follow is disabled and the human point
	 * moves to the camera-relative target.
	 * Alt-wheel multiplies the retained distance by 1.2 per wheel unit and clamps
	 * it to 0.5–10,000 world units. Releasing the button or disabling this action
	 * restores the ground-follow value saved when capture began.
	 */
	void tickHumanDragTool(World& world, ControllerSystem& controller,
		const RaycastHit& hit, bool enabled, int mouse_state, float scroll_delta, bool alt_pressed)
	{
		const bool mouse_down = mouse_state == button_states::press || mouse_state == button_states::hold;
		if(!enabled || !mouse_down)
		{
			clearHumanDrag(world, controller);
			return;
		}

		if(mouse_state == button_states::press)
		{
			clearHumanDrag(world, controller);
			const glm::dvec3 ray_origin = world.camera.getWorldPosition();
			const glm::dvec3 ray_direction = glm::normalize(glm::dvec3(world.camera.getFront()));
			double closest_distance = hit.hit ? hit.distance : 10000.0;
			for(const HumanData& human : world.humans.getHumans())
			{
				if(!world.world_body_data.isWorldBodyActive(human.world_body_id)) continue;
				const WorldBody& body = world.world_body_data.getWorldBody(human.world_body_id);
				const glm::dvec3 center = world.humans.getWorldPosition(human);
				const glm::dvec3 up = glm::normalize(body.rotation * glm::dvec3(human.up));
				double distance = 0.0;
				const double radius = static_cast<double>(human.getHeight()) * 0.14;
				if(intersectRayCapsule(ray_origin, ray_direction, center, up,
					human.getHeight(), radius, distance) && distance < closest_distance)
				{
					closest_distance = distance;
					controller.dragged_human_id = human.id;
					controller.dragged_human_distance = distance;
					controller.dragged_human_followed_ground = human.follow_ground;
				}
			}
			if(HumanData* human = world.humans.findHuman(controller.dragged_human_id)) human->follow_ground = false;
		}

		if(controller.dragged_human_id == no_human) return;
		HumanData* human = world.humans.findHuman(controller.dragged_human_id);
		if(human == nullptr)
		{
			clearHumanDrag(world, controller);
			return;
		}

		if(alt_pressed && scroll_delta != 0.0f)
		{
			controller.dragged_human_distance *= std::pow(1.2, static_cast<double>(scroll_delta));
			controller.dragged_human_distance = glm::clamp(controller.dragged_human_distance, 0.5, 10000.0);
		}
		if(!world.world_body_data.isWorldBodyActive(human->world_body_id))
		{
			clearHumanDrag(world, controller);
			return;
		}
		const WorldBody& body = world.world_body_data.getWorldBody(human->world_body_id);
		const glm::dvec3 target = world.camera.getWorldPosition()
			+ glm::dvec3(world.camera.getFront()) * controller.dragged_human_distance;
		const glm::dvec3 body_local_target = glm::conjugate(body.rotation) * (target - body.position);
		human->body_local_position = body_local_target;
	}
}
