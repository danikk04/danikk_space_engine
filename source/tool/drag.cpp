#include <danikk_space_engine/tool.h>
#include <danikk_space_engine/systems/controller/controller.h>
#include <danikk_space_engine/systems/planet/system.h>
#include <danikk_space_engine/world.h>
#include <danikk_engine/input.h>

#include <cmath>

namespace danikk_space_engine
{
	namespace
	{
		constexpr WorldBodyId no_dragged_body = static_cast<WorldBodyId>(-1);

		/**
		 * @brief Checks whether the body is eligible for the drag tool and has loaded geometry.
		 */
		bool canDragWorldBody(const World& world, WorldBodyId body_id)
		{
			if(!world.world_body_data.isWorldBodyActive(body_id)) return false;

			if(world.camera.isActive() && world.camera.getWorldBodyId() == body_id)
			{
				return false;
			}

			if(world.planets.isPlanet(body_id)) return false;
			return true;
		}

		/**
		 * @brief Clears drag.
		 */
		void clearDrag(ControllerSystem& controller)
		{
			controller.dragged_world_body_id = no_dragged_body;
			controller.dragged_body_distance = 0.0;
			controller.dragged_hit_offset = glm::dvec3(0.0);
		}
	}

	/**
	 * @brief Updates capture, distance, and rotation when the unified Drag tool targets a WorldBody.
	 * @param world World providing camera transforms and target body resources.
	 * @param controller Local controller storing the captured body and local hit offset.
	 * @param hit Current raycast hit used to begin capture on a mouse press.
	 * @param enabled Whether the unified Drag tool should process a WorldBody capture.
	 * @param mouse_state Current left mouse button state; release or disable clears capture.
	 * @param scroll_delta Wheel movement supplied while the mouse button is held.
	 * @param alt_pressed Changes wheel input from rotation to grab-distance adjustment.
	 * @param cursor_delta Mouse displacement used to rotate around camera-relative axes.
	 * On capture, stores the hit point relative to the body. The body origin is
	 * recomputed after rotation or distance changes so that the captured local
	 * point remains on the camera ray. Planet bodies and the body owning the camera
	 * are ineligible. Alt-wheel scales grab distance by 1.2 per wheel unit and
	 * clamps it to 0.5–10,000 world units.
	 * Normal wheel input rotates the body; Alt-wheel changes grab distance, and mouse movement
	 * rotates around camera-relative axes.
	 */
	void tickDragTool(
		World& world,
		ControllerSystem& controller,
		const RaycastHit& hit,
		bool enabled,
		int mouse_state,
		float scroll_delta,
		bool alt_pressed,
		const vec2& cursor_delta)
	{
		const bool mouse_down = mouse_state == button_states::press || mouse_state == button_states::hold;
		if(!enabled || !mouse_down)
		{
			clearDrag(controller);
			return;
		}

		if(mouse_state == button_states::press)
		{
			clearDrag(controller);
			if(hit.hit && world.world_body_data.isWorldBodyActive(hit.body_id))
			{
				if(canDragWorldBody(world, hit.body_id))
				{
					const WorldBody& body = world.world_body_data.getWorldBody(hit.body_id);
					controller.dragged_world_body_id = hit.body_id;
					controller.dragged_body_distance = hit.distance;
					controller.dragged_hit_offset = glm::conjugate(body.rotation)
						* (hit.position - body.position);
				}
			}
		}

		const WorldBodyId body_id = controller.dragged_world_body_id;
		if(body_id == no_dragged_body) return;

		if(!canDragWorldBody(world, body_id))
		{
			clearDrag(controller);
			return;
		}

		WorldBody& body = world.world_body_data.getWorldBody(body_id);
		if(alt_pressed && scroll_delta != 0.0f)
		{
			controller.dragged_body_distance *= std::pow(1.2, static_cast<double>(scroll_delta));
			controller.dragged_body_distance = glm::clamp(controller.dragged_body_distance, 0.5, 10000.0);
		}
		else if(!alt_pressed && scroll_delta != 0.0f)
		{
			const glm::dvec3 axis = glm::normalize(glm::dvec3(world.camera.getUp()));
			constexpr double half_angle_per_scroll = 3.14159265358979323846 / 24.0;
			const double half_angle = static_cast<double>(scroll_delta) * half_angle_per_scroll;
			const double sine = std::sin(half_angle);
			const glm::dquat camera_rotation(std::cos(half_angle), axis.x * sine, axis.y * sine, axis.z * sine);
			body.rotation = glm::normalize(camera_rotation * body.rotation);
		}
		if(alt_pressed && (cursor_delta.x != 0.0f || cursor_delta.y != 0.0f))
		{
			const glm::dvec3 camera_up = glm::normalize(glm::dvec3(world.camera.getUp()));
			const glm::dvec3 camera_right = glm::normalize(glm::dvec3(world.camera.getRight()));
			const double yaw_half_angle = -static_cast<double>(cursor_delta.x) * 0.5;
			const double pitch_half_angle = -static_cast<double>(cursor_delta.y) * 0.5;
			const glm::dquat yaw_rotation(
				std::cos(yaw_half_angle),
				camera_up.x * std::sin(yaw_half_angle),
				camera_up.y * std::sin(yaw_half_angle),
				camera_up.z * std::sin(yaw_half_angle));
			const glm::dquat pitch_rotation(
				std::cos(pitch_half_angle),
				camera_right.x * std::sin(pitch_half_angle),
				camera_right.y * std::sin(pitch_half_angle),
				camera_right.z * std::sin(pitch_half_angle));
			body.rotation = glm::normalize(pitch_rotation * yaw_rotation * body.rotation);
		}

		const glm::dvec3 target = world.camera.getWorldPosition()
			+ glm::dvec3(world.camera.getFront()) * controller.dragged_body_distance;
		body.position = target - body.rotation * controller.dragged_hit_offset;
	}
}
