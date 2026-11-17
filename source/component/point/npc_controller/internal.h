#pragma once
#include <danikk_space_engine/component/point/human/npc_controller.h>
#include <danikk_space_engine/component/point/human/human.h>
namespace danikk_space_engine::npc_internal
{
	void updateNpcChunk(World&, PointId, ChunkId, NpcControllerComponent&);
	bool validEnemy(World&, const NpcTarget&, FactionId);
	void collectCandidates(World&, PointId, ChunkId, FactionId, NpcControllerComponent&);
	bool updateCombat(World&, PointId, ChunkId, NpcControllerComponent&, HumanComponent&, float);
	/** @brief Updates the movement command and optional camera tracking. */
	void updateNpcMovement(World&, PointComponentSlot&, NpcControllerComponent&, HumanComponent&, float);
}
