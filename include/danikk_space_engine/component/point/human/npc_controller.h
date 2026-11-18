#pragma once

#include <danikk_space_engine/id.h>
#include <danikk_space_engine/component/point/point.h>
#include <danikk_space_engine/fwd.h>

namespace danikk_space_engine
{
	/** @brief Candidate identity validated against both its component slot and point. */
	struct NpcTarget
	{
		PointComponentSlotId slot_id = static_cast<PointComponentSlotId>(-1);
		PointId point_id = static_cast<PointId>(-1);
		uint64 generation_id = 0; ///< Incarnation of the human component slot.
	};

	/** @brief Executable commands assigned to an NPC by UnitSystem. */
	struct UnitOrder
	{
		bool has_move_target = false; ///< Whether to move toward move_target_world.
		glm::dvec3 move_target_world = glm::dvec3(0.0); ///< World position to approach.
		bool has_look_target = false; ///< Whether to orient toward look_target_world.
		glm::dvec3 look_target_world = glm::dvec3(0.0); ///< World position to face.
		bool has_attack_target = false; ///< Whether target identity describes the active combat order.
		PointComponentSlotId target_slot_id = static_cast<PointComponentSlotId>(-1); ///< Target human slot.
		PointId target_point_id = static_cast<PointId>(-1); ///< Target point identity.
		uint64 target_generation_id = 0; ///< Target slot incarnation, preventing stale recycled IDs.
		bool has_equip_order = false; ///< Whether to equip equip_item_definition_id.
		ItemDefinitionId equip_item_definition_id = 0; ///< Item selected by UnitSystem for equipment.
		bool should_fire = false; ///< Whether to invoke the selected weapon this tick.
		ItemDefinitionId fire_item_definition_id = 0; ///< Equipped weapon selected for firing.

		/** @brief Clears all commands and target identity. */
		void clear()
		{
			has_move_target = false;
			move_target_world = glm::dvec3(0.0);
			has_look_target = false;
			look_target_world = glm::dvec3(0.0);
			has_attack_target = false;
			target_slot_id = static_cast<PointComponentSlotId>(-1);
			target_point_id = static_cast<PointId>(-1);
			target_generation_id = 0;
			has_equip_order = false;
			equip_item_definition_id = 0;
			should_fire = false;
			fire_item_definition_id = 0;
		}
	};

	/**
	 * @brief Stores the unit's assigned order and the human component it controls.
	 *
	 * The controller is attached to the same point and chunk as its HumanComponent. UnitSystem owns
	 * candidate scanning and tactical decisions; this component stores those results and executes
	 * movement, look, equipment, and firing commands from its UnitOrder.
	 */
	struct NpcControllerComponent
	{
		/** @brief Human component slot controlled by this NPC controller. */
		PointComponentSlotId human_slot_id = static_cast<PointComponentSlotId>(-1);
		/** @brief Pauses order execution while preserving the assigned order. */
		bool enabled = true;
		/** @brief Candidate cache populated and consumed by UnitSystem, never by the controller tick. */
		DynamicArray<NpcTarget> enemy_candidates;
		/** @brief Next cached candidate awaiting UnitSystem visibility evaluation. */
		size_t candidate_index = 0;
		/** @brief Commands selected centrally and executed by this component. */
		UnitOrder order;

	};

	/**
	 * @brief Creates a human and attaches an order-driven controller to the same point.
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
	 * @brief Validates the paired human and executes its current UnitSystem order.
	 * @param slot Controller slot; its point and chunk must match the referenced human slot.
	 * @param world World used to resolve component ownership and camera position.
	 */
	void tickNpcControllerComponent(PointComponentSlot& slot, World& world);
	/** @brief Updates the owning chunk of an active NPC after component traversal. */
	void updateNpcChunkAfterTick(World& world, PointComponentSlotId slot_id);
}
