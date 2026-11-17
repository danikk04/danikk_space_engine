#pragma once

#include <danikk_space_engine/id.h>
#include <danikk_space_engine/component/point/point.h>

namespace danikk_space_engine
{
	/** @brief Candidate identity validated against both its component slot and point. */
	struct NpcTarget
	{
		PointComponentSlotId slot_id = static_cast<PointComponentSlotId>(-1);
		PointId point_id = static_cast<PointId>(-1);
		uint64 generation_id = 0; ///< Incarnation of the human component slot.
		float visibility_age = 0.0f; ///< Seconds since the last successful visibility check.
	};

	/**
	 * @brief Stores a human's movement target, optional camera tracking, and combat state.
	 *
	 * The controller is attached to the same point and chunk as its HumanComponent. Each tick
	 * writes a one-tick velocity command to that human; the human component handles movement,
	 * gravity, and ground support separately.
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
		/** @brief Candidate list built from nearby chunks and checked over successive ticks. */
		DynamicArray<NpcTarget> enemy_candidates;
		/** @brief Next candidate awaiting its visibility raycast. */
		size_t candidate_index = 0;
		/** @brief Up to three currently visible enemy identities. */
		NpcTarget targeted_enemy[3]{};
		/** @brief Index of the target currently being engaged. */
		uint8 target_index = 0;
		/** @brief Seconds accumulated toward switching targets. */
		float target_switch_timer = 0.0f;

		/**
		 * @brief Sets an explicit local-space movement destination.
		 * @param position Destination in the same chunk-local frame as the controlled human.
		 */
		void moveTo(const vec3& position)
		{
			target_position = position;
			has_movement_target = true;
		}

		/**
		 * @brief Clears the pending movement target.
		 *
		 * The controller remains attached and can be re-enabled or given another target later.
		 */
		void stop()
		{
			has_movement_target = false;
		}
	};

	/**
	 * @brief Creates a human and attaches an autonomous controller to the same point.
	 * @param world World receiving the human, chunk, and controller resources.
	 * @param chunk_id Existing chunk that owns the new character.
	 * @param position Initial world-body-local position of the character's feet.
	 * @param up Initial local up axis for the human.
	 * @param forward Initial local facing axis for the human.
	 * @return ID of the created human component slot.
	 */
	PointComponentSlotId createNpc(World& world, ChunkId chunk_id,
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
	/** @brief Updates the owning chunk of an active NPC after component traversal. */
	void updateNpcChunkAfterTick(World& world, PointComponentSlotId slot_id);
}
