#pragma once
#include <danikk_space_engine/component/point/human/npc_controller.h>
#include <danikk_space_engine/component/point/human/human.h>
namespace danikk_space_engine::npc_internal
{
	void updateNpcChunk(World&, PointId, ChunkId, NpcControllerComponent&);
	bool validEnemy(World&, const NpcTarget&, FactionId);
	void collectCandidates(World&, PointId, ChunkId, FactionId, NpcControllerComponent&);
	/** @brief Executes the movement and look commands assigned by UnitSystem. */
	void executeNpcMovementOrder(World&, PointComponentSlot&, NpcControllerComponent&, HumanComponent&, float);
	/** @brief Executes the equipment and firing commands assigned by UnitSystem. */
	void executeNpcCombatOrder(World&, PointComponentSlot&, NpcControllerComponent&);
}
