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
		float age = 0.0f;
	};

	/** @brief Per-faction tactical memory and processing state. */
	struct UnitSystem
	{
		FactionId faction_id = static_cast<FactionId>(-1);
		DynamicArray<UnitSighting> known_sightings;
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
		/** @brief Refreshes a faction contact or appends a newly observed target. */
		void rememberSighting(DynamicArray<UnitSighting>&, FactionId, const NpcTarget&, const glm::dvec3&);
		/** @brief Resolves an active controller and its paired living human. */
		bool getActiveNpc(World&, PointComponentSlotId, PointComponentSlot*&, NpcControllerComponent*&, HumanComponent*&);
		/** @brief Updates faction contacts by scanning candidates and raycasting visible targets. */
		void updatePerception(World&, UnitSystem&, const DynamicArray<PointComponentSlotId>&, float);
		/** @brief Assigns orders to all active NPC controllers. */
		void assignOrders(World&, UnitSystem&, const DynamicArray<PointComponentSlotId>&);
	}
}
