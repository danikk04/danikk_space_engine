#pragma once

#include <danikk_space_engine/id.h>
#include <danikk_space_engine/component/point/point.h>

namespace danikk_space_engine
{

	/**
	 * @brief Stores a human's movement target and deterministic wandering/look state.
	 *
	 * The controller is attached to the same point and chunk as its HumanComponent. Each tick
	 * writes a one-tick velocity command to that human; the human component handles movement,
	 * gravity, and ground support separately. Autonomous wandering alternates idle waits with
	 * disk-sampled destinations around the initial position. Each controller owns its deterministic
	 * pseudorandom state, so NPCs do not share or reseed a global random generator.
	 */
	struct NpcControllerComponent
	{
		/** @brief Human component slot controlled by this NPC controller. */
		PointComponentSlotId human_slot_id = static_cast<PointComponentSlotId>(-1);
		/** @brief Current movement destination in the human's chunk-local coordinate system. */
		vec3 target_position = vec3(0.0f);
		/** @brief Whether the controller currently has a destination to pursue; false means idle. */
		bool has_movement_target = false;
		/** @brief If enabled, rotate head yaw/pitch toward the active camera instead of random looks. */
		bool look_at_camera = false;
		/** @brief Pauses command generation while preserving the component and its state. */
		bool enabled = true;
		/** @brief Enables autonomous trips around `wander_center`; disabled by explicit moveTo(). */
		bool wander_enabled = true;
		/** @brief Indicates that the initial local wander center has been captured. */
		bool wander_initialized = false;
		/** @brief Local origin around which autonomous destinations are sampled. */
		vec3 wander_center = vec3(0.0f);
		/** @brief Maximum horizontal distance of randomly selected wander destinations. */
		float wander_radius = 4.0f;
		/** @brief Remaining wait or destination timeout before choosing the next wandering action. */
		float wander_timer = 0.0f;
		/** @brief Time until the next random head-look target is selected. */
		float look_timer = 0.0f;
		/** @brief Current target head yaw and pitch for smooth look interpolation, in radians. */
		vec2 look_target = vec2(0.0f);
		/** @brief Per-controller xorshift state, initialized from point and component IDs. */
		uint32 random_state = 0;

		/**
		 * @brief Replaces autonomous wandering with an explicit local-space movement destination.
		 * @param position Destination in the same chunk-local frame as the controlled human.
		 */
		void moveTo(const vec3& position)
		{
			wander_enabled = false;
			target_position = position;
			has_movement_target = true;
		}

		/**
		 * @brief Cancels wandering and clears the pending movement target.
		 *
		 * The controller remains attached and can be re-enabled or given another target later.
		 */
		void stop()
		{
			wander_enabled = false;
			has_movement_target = false;
		}
	};

	/**
	 * @brief Creates a human and attaches an autonomous controller to the same point.
	 * @param world World receiving the human, chunk, and controller resources.
	 * @param world_body_id Body that owns the new character.
	 * @param position Initial world-body-local position of the character's feet.
	 * @param up Initial local up axis for the human.
	 * @param forward Initial local facing axis for the human.
	 * @return ID of the created human component slot.
	 */
	PointComponentSlotId createNpc(World& world, WorldBodyId world_body_id,
		const glm::dvec3& position, const vec3& up, const vec3& forward);

	/**
	 * @brief Placement-constructs an NPC controller from optional initial behavior state.
	 * @param destination Controller payload storage supplied by the slot pool.
	 * @param init_data Optional controller state to copy; null uses default behavior.
	 */
	void constructNpcControllerComponent(void* destination, const void* init_data);

	/**
	 * @brief Move-constructs an NPC controller during point-slot relocation.
	 * @param destination Uninitialized replacement payload storage.
	 * @param source Existing payload, destroyed after the move.
	 */
	void moveNpcControllerComponent(void* destination, void* source);

	/**
	 * @brief Runs the NPC controller destructor without freeing slot-owned storage.
	 * @param data Address of the live payload.
	 */
	void destroyNpcControllerComponent(void* data);

	/**
	 * @brief Validates the paired human, chooses movement/look targets, and writes the next human command.
	 * @param slot Controller slot; its point and chunk must match the referenced human slot.
	 * @param world World used to resolve component ownership and camera position.
	 */
	void tickNpcControllerComponent(PointComponentSlot& slot, World& world);
}
