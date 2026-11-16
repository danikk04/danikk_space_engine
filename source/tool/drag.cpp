#include <danikk_space_engine/tool.h>
#include <danikk_space_engine/component/point/local_player_controller.h>
#include <danikk_space_engine/component/world_body/planet/planet.h>
#include <danikk_space_engine/world/world.h>
#include <danikk_engine/input.h>

#include <cmath>

namespace danikk_space_engine
{
	namespace
	{
		constexpr WorldBodyId no_dragged_body = static_cast<WorldBodyId>(-1);

		/**
		 * @brief Reports whether drag world body.
		 */
		bool canDragWorldBody(const World& world, WorldBodyId body_id)
		{
			if(!world.data.isWorldBodyActive(body_id)) return false;

			const PointComponentSlot& camera_slot = world.getCameraSlot();
			if(world.data.isChunkActive(camera_slot.chunk_id)
				&& world.data.getChunk(camera_slot.chunk_id).world_body_id == body_id)
			{
				return false;
			}

			const WorldBody& body = world.data.getWorldBody(body_id);
			for(WorldBodyComponentSlotId slot_id : body.component_slot_ids)
			{
				if(!world.data.isWorldBodyComponentSlotActive(slot_id)) continue;
				if(world.data.getWorldBodyComponentSlot(slot_id).type_id == WorldBodyComponentTypeId::planet)
				{
					return false;
				}
			}
			return true;
		}

		/**
		 * @brief Clears drag.
		 */
		void clearDrag(LocalPlayerControllerComponent& controller)
		{
			controller.dragged_world_body_id = no_dragged_body;
			controller.dragged_body_distance = 0.0;
			controller.dragged_hit_offset = glm::dvec3(0.0);
		}
	}

	// Captures the body under the cursor on mouse press, then keeps the grabbed local point at
	// its current camera distance while the camera moves. Normal scrolling rotates the body; with
	// Alt, scrolling changes the distance and cursor movement rotates it around camera-relative axes.
	// After each transform, body position is adjusted to keep the grabbed point on the view ray.

	/**
	 * @brief Runs one simulation tick for drag tool.
	 */
	void tickDragTool(
		World& world,
		LocalPlayerControllerComponent& controller,
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
			if(hit.hit && world.data.isChunkActive(hit.face.chunk_id))
			{
				const Chunk& chunk = world.data.getChunk(hit.face.chunk_id);
				if(canDragWorldBody(world, chunk.world_body_id))
				{
					const WorldBody& body = world.data.getWorldBody(chunk.world_body_id);
					controller.dragged_world_body_id = chunk.world_body_id;
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

		WorldBody& body = world.data.getWorldBody(body_id);
		if(alt_pressed && scroll_delta != 0.0f)
		{
			controller.dragged_body_distance *= std::pow(1.2, static_cast<double>(scroll_delta));
			controller.dragged_body_distance = glm::clamp(controller.dragged_body_distance, 0.5, 10000.0);
		}
		else if(!alt_pressed && scroll_delta != 0.0f)
		{
			const glm::dvec3 axis = glm::normalize(glm::dvec3(world.getCameraComponent().getUp()));
			constexpr double half_angle_per_scroll = 3.14159265358979323846 / 24.0;
			const double half_angle = static_cast<double>(scroll_delta) * half_angle_per_scroll;
			const double sine = std::sin(half_angle);
			const glm::dquat camera_rotation(std::cos(half_angle), axis.x * sine, axis.y * sine, axis.z * sine);
			body.rotation = glm::normalize(camera_rotation * body.rotation);
		}
		if(alt_pressed && (cursor_delta.x != 0.0f || cursor_delta.y != 0.0f))
		{
			const glm::dvec3 camera_up = glm::normalize(glm::dvec3(world.getCameraComponent().getUp()));
			const glm::dvec3 camera_right = glm::normalize(glm::dvec3(world.getCameraComponent().getRight()));
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

		const glm::dvec3 target = world.getCameraWorldPosition()
			+ glm::dvec3(world.getCameraComponent().getFront()) * controller.dragged_body_distance;
		body.position = target - body.rotation * controller.dragged_hit_offset;
	}
}
