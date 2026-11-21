#pragma once

#include <danikk_space_engine/systems/unit/internal/state.h>

namespace danikk_space_engine::unit_system_internal
{
	/** @brief Checks that a cached target is a living hostile human. */
	bool isValidEnemy(World&, const UnitTarget&, FactionId);
	/** @brief Rebuilds the body-local sparse octree used for nearby-human queries. */
	void rebuildUnitSpatialIndex(World&, UnitSpatialIndex&);
	/** @brief Collects indexed human slots in a body-local box. */
	void collectNearbyHumanSlots(const UnitSpatialIndex&, WorldBodyId,
		const glm::dvec3&, double, DynamicArray<PointComponentSlotId>&);
	/** @brief Rebuilds a unit's hostile-human candidate list from nearby octree leaves. */
	void collectEnemyTargets(World&, ChunkId, FactionId,
		const UnitSpatialIndex&, UnitAgentState&);
	/** @brief Checks whether a shared contact still identifies a living hostile. */
	bool isValidSighting(World&, const UnitSighting&);
	/** @brief Checks whether a sighting remains within the tactical memory window. */
	bool isRecentSighting(const UnitSighting&);
	/** @brief Refreshes a faction contact or appends a newly observed target. */
	void rememberSighting(World&, UnitGroup&, FactionId, const UnitTarget&, const glm::dvec3&);
	/** @brief Updates visibility for a faction's known contacts. */
	void updatePerception(World&, UnitGroup&, const UnitSpatialIndex&);
	/** @brief Records lost contacts, expires stale intel, and tracks visible corpse locations. */
	void updateTacticalMemory(World&, UnitGroup&);
}
