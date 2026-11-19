#pragma once

#include <danikk_space_engine/systems/unit/unit.h>
#include <danikk_space_engine/component/point/human/human.h>

namespace danikk_space_engine
{
	/** @brief Pooled point-component identity used for safe enemy references. */
	struct UnitTarget
	{
		PointComponentSlotId slot_id = static_cast<PointComponentSlotId>(-1);
		PointId point_id = static_cast<PointId>(-1);
		uint64 generation_id = 0;
	};

	/** @brief Current action values assigned by UnitGroup to one unit. */
	struct UnitOrder
	{
		bool has_move_target = false;
		glm::dvec3 move_target_world = glm::dvec3(0.0);
		bool has_look_target = false;
		glm::dvec3 look_target_world = glm::dvec3(0.0);
		bool has_attack_target = false;
		PointComponentSlotId target_slot_id = static_cast<PointComponentSlotId>(-1);
		PointId target_point_id = static_cast<PointId>(-1);
		uint64 target_generation_id = 0;
		bool has_equip_order = false;
		ItemDefinitionId equip_item_definition_id = static_cast<ItemDefinitionId>(-1);
		bool should_fire = false;
		ItemDefinitionId fire_item_definition_id = static_cast<ItemDefinitionId>(-1);

		/** @brief Clears actions and invalidates any current target. */
		void clear()
		{
			has_move_target = false;
			has_look_target = false;
			has_attack_target = false;
			target_slot_id = static_cast<PointComponentSlotId>(-1);
			target_point_id = static_cast<PointId>(-1);
			target_generation_id = 0;
			has_equip_order = false;
			equip_item_definition_id = static_cast<ItemDefinitionId>(-1);
			should_fire = false;
			fire_item_definition_id = static_cast<ItemDefinitionId>(-1);
		}
	};

	/** @brief Central AI state and current order for one registered human. */
	struct UnitAgentState
	{
		PointComponentSlotId human_slot_id = static_cast<PointComponentSlotId>(-1);
		uint64 human_generation_id = 0;
		bool enabled = true;
		UnitGlobalState global_state = UnitGlobalState::rest;
		UnitSubstate substate = UnitSubstate::idle;
		DynamicArray<UnitTarget> enemy_candidates;
		size_t candidate_index = 0;
		UnitOrder order;
	};

	/** @brief One faction's report that a hostile unit was seen. */
	struct UnitSighting
	{
		FactionId faction_id = static_cast<FactionId>(-1);
		PointComponentSlotId target_slot_id = static_cast<PointComponentSlotId>(-1);
		PointId target_point_id = static_cast<PointId>(-1);
		uint64 target_generation_id = 0;
		glm::dvec3 position = glm::dvec3(0.0);
		WorldBodyId body_id = static_cast<WorldBodyId>(-1);
		glm::dvec3 body_local_position = glm::dvec3(0.0);
		uint32 last_seen_timestamp_ms = 0;
		uint32 last_attack_timestamp_ms = 0;
		PointId last_attacked_point_id = static_cast<PointId>(-1);
		bool visible_last_tick = false;
		bool visible_this_tick = false;
		bool visibility_checked_this_tick = false;
	};

	/** @brief Remembered hostile contact or known corpse location awaiting a unit visit. */
	struct UnitSearchLocation
	{
		WorldBodyId body_id = static_cast<WorldBodyId>(-1);
		glm::dvec3 body_local_position = glm::dvec3(0.0);
		PointComponentSlotId target_slot_id = static_cast<PointComponentSlotId>(-1);
		uint64 target_generation_id = 0;
		PointComponentSlotId assigned_unit_slot_id = static_cast<PointComponentSlotId>(-1);
		bool is_corpse = false;
	};

	/** @brief Tick-local candidate data reused while assigning one faction's search orders. */
	struct UnitSearchCandidate
	{
		PointComponentSlotId slot_id = static_cast<PointComponentSlotId>(-1);
		PointComponentSlot* slot = nullptr;
		UnitAgentState* agent = nullptr;
		HumanComponent* human = nullptr;
		WorldBodyId body_id = static_cast<WorldBodyId>(-1);
		glm::dvec3 position = glm::dvec3(0.0);
		bool assigned_to_search = false;
	};

	/** @brief Persistent post-combat position and scan phase assigned to one unit. */
	struct UnitPostCombatAssignment
	{
		PointComponentSlotId unit_slot_id = static_cast<PointComponentSlotId>(-1);
		uint64 unit_generation_id = 0;
		WorldBodyId body_id = static_cast<WorldBodyId>(-1);
		glm::dvec3 body_local_position = glm::dvec3(0.0);
		float scan_angle = 0.0f;
		uint32 next_scan_timestamp_ms = 0;
	};

	/** @brief Per-faction tactical memory and processing state. */
	struct UnitGroup
	{
		FactionId faction_id = static_cast<FactionId>(-1);
		DynamicArray<UnitSighting> known_sightings;
		DynamicArray<UnitAgentState> agents;
		DynamicArray<UnitSearchLocation> search_locations;
		/** @brief Reusable temporary storage; pointers are valid only during assignSearchOrders(). */
		DynamicArray<UnitSearchCandidate> search_candidates;
		/** @brief Stable dispersal destinations and scan phases while the faction is reconnoitring. */
		DynamicArray<UnitPostCombatAssignment> post_combat_assignments;
		/** @brief Group-wide time since any faction unit last saw or reported a hostile. */
		float time_since_enemy_visible = 0.0f;
		bool enemy_seen_this_tick = false;
		bool attack_reported_this_tick = false;
		bool has_had_enemy_contact = false;
		bool war_active = false;
	};

	/** @brief Private storage owned by one World::unit_system instance. */
	struct UnitSystem::Impl
	{
		DynamicArray<UnitGroup> groups;
	};

	namespace unit_system_internal
	{
		/** @brief Checks that a cached target is still a living hostile human. */
		bool isValidEnemy(World&, const UnitTarget&, FactionId);
		/** @brief Rebuilds a unit's nearby hostile-human candidate list. */
		void collectEnemyTargets(World&, PointId, ChunkId, FactionId, UnitAgentState&);
		/** @brief Checks that a shared contact still names a living hostile human. */
		bool isValidSighting(World&, const UnitSighting&);
		/** @brief Reports whether the last observation is no older than the contact timeout. */
		bool isRecentSighting(const UnitSighting&);
		/** @brief Refreshes a faction contact or appends a newly observed target. */
		void rememberSighting(World&, UnitGroup&, FactionId, const UnitTarget&, const glm::dvec3&);
		/** @brief Records contacts that transitioned from visible to lost and updates combat inactivity. */
		void finishPerception(World&, UnitGroup&, float);
		/** @brief Resolves a registered live human and its point slot. */
		bool getActiveUnit(World&, UnitAgentState&, PointComponentSlot*&, HumanComponent*&);
		/** @brief Updates faction contacts by scanning candidates and raycasting visible targets. */
		void updatePerception(World&, UnitGroup&);
		/** @brief Assigns orders to all active agents in a faction. */
		void assignOrders(World&, UnitGroup&);
		/** @brief Assigns idle units to inspect lost-contact locations after combat ends. */
		void assignSearchOrders(World&, UnitGroup&);
		/** @brief Disperses idle units and periodically changes their look direction after combat. */
		void assignPostCombatOrders(World&, UnitGroup&);
		/** @brief Executes stored orders and migrates units after component traversal. */
		void executeUnitOrders(World&, UnitGroup&);
		/** @brief Moves a unit point and all attached slots into its newly occupied loaded chunk. */
		void updateUnitChunk(World&, PointComponentSlot&);
	}
}
