#pragma once

#include <danikk_space_engine/unit_system.h>
#include <danikk_space_engine/component/point/human/npc_controller.h>
#include <danikk_space_engine/component/point/human/human.h>

namespace danikk_space_engine
{
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

	/** @brief Last-known hostile location awaiting a post-combat inspection. */
	struct UnitSearchLocation
	{
		WorldBodyId body_id = static_cast<WorldBodyId>(-1);
		glm::dvec3 body_local_position = glm::dvec3(0.0);
		PointComponentSlotId target_slot_id = static_cast<PointComponentSlotId>(-1);
		uint64 target_generation_id = 0;
		PointComponentSlotId assigned_controller_slot_id = static_cast<PointComponentSlotId>(-1);
	};

	/** @brief Tick-local candidate data reused while assigning one faction's search orders. */
	struct UnitSearchCandidate
	{
		PointComponentSlotId slot_id = static_cast<PointComponentSlotId>(-1);
		PointComponentSlot* slot = nullptr;
		NpcControllerComponent* controller = nullptr;
		HumanComponent* human = nullptr;
		WorldBodyId body_id = static_cast<WorldBodyId>(-1);
		glm::dvec3 position = glm::dvec3(0.0);
		bool assigned_to_search = false;
	};

	/** @brief Persistent post-combat position and scan phase assigned to one unit. */
	struct UnitPostCombatAssignment
	{
		PointComponentSlotId controller_slot_id = static_cast<PointComponentSlotId>(-1);
		uint64 controller_generation_id = 0;
		WorldBodyId body_id = static_cast<WorldBodyId>(-1);
		glm::dvec3 body_local_position = glm::dvec3(0.0);
		float scan_angle = 0.0f;
		uint32 next_scan_timestamp_ms = 0;
	};

	/** @brief Per-faction tactical memory and processing state. */
	struct UnitSystem
	{
		FactionId faction_id = static_cast<FactionId>(-1);
		DynamicArray<UnitSighting> known_sightings;
		DynamicArray<UnitSearchLocation> search_locations;
		/** @brief Reusable temporary storage; pointers are valid only during assignSearchOrders(). */
		DynamicArray<UnitSearchCandidate> search_candidates;
		/** @brief Stable dispersal destinations and scan phases while the faction is out of combat. */
		DynamicArray<UnitPostCombatAssignment> post_combat_assignments;
		float time_since_enemy_visible = 0.0f;
		bool enemy_seen_this_tick = false;
		bool attack_reported_this_tick = false;
		bool has_had_enemy_contact = false;
		bool enemy_contact_this_tick = false;
	};

	namespace unit_system_internal
	{
		/** @brief Releases all faction-system state when the world is reset or shut down. */
		void resetUnitSystems();
		/** @brief Checks that a cached target is still a living hostile human. */
		bool isValidEnemy(World&, const NpcTarget&, FactionId);
		/** @brief Rebuilds a unit's nearby hostile-human candidate list. */
		void collectEnemyCandidates(World&, PointId, ChunkId, FactionId, NpcControllerComponent&);
		/** @brief Checks that a shared contact still names a living hostile human. */
		bool isValidSighting(World&, const UnitSighting&);
		/** @brief Reports whether the last observation is no older than the contact timeout. */
		bool isRecentSighting(const UnitSighting&);
		/** @brief Refreshes a faction contact or appends a newly observed target. */
		void rememberSighting(World&, UnitSystem&, FactionId, const NpcTarget&, const glm::dvec3&);
		/** @brief Records contacts that transitioned from visible to lost and updates combat inactivity. */
		void finishPerception(World&, UnitSystem&, float);
		/** @brief Resolves an active controller and its paired living human. */
		bool getActiveNpc(World&, PointComponentSlotId, PointComponentSlot*&, NpcControllerComponent*&, HumanComponent*&);
		/** @brief Updates faction contacts by scanning candidates and raycasting visible targets. */
		void updatePerception(World&, UnitSystem&, const DynamicArray<PointComponentSlotId>&);
		/** @brief Assigns orders to all active NPC controllers. */
		void assignOrders(World&, UnitSystem&, const DynamicArray<PointComponentSlotId>&);
		/** @brief Assigns idle units to inspect lost-contact locations after combat ends. */
		void assignSearchOrders(World&, UnitSystem&, const DynamicArray<PointComponentSlotId>&);
		/** @brief Disperses idle units and periodically changes their look direction after combat. */
		void assignPostCombatOrders(World&, UnitSystem&, const DynamicArray<PointComponentSlotId>&);
	}
}
