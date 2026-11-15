#include <cmath>

#include <danikk_space_engine/component/point/npc_controller.h>
#include <danikk_space_engine/component/point/character.h>
#include <danikk_space_engine/world/world.h>

namespace danikk_space_engine
{
	// Creates an NPC in a separate component chunk so unloading generated terrain does not delete it.
	// Input position/orientation are body-local. Returns the controller slot for later commands.
	PointComponentSlotId createNpc(World& world, WorldBodyId world_body_id,
		const glm::dvec3& position, const vec3& up, const vec3& forward)
	{
		if(!world.data.isWorldBodyActive(world_body_id)) formatFatalError("Cannot create NPC on an invalid body");
		Chunk& chunk = world.data.createChunk(world_body_id);
		chunk.position = position;
		const ChunkId chunk_id = chunk.chunk_id;
		const PointId point_id = world.data.createPoint();
		CharacterComponent character;
		character.up = glm::length(up) > 1e-6f ? glm::normalize(up) : vec3(0, 0, 1);
		character.forward = forward - character.up * glm::dot(forward, character.up);
		if(glm::length(character.forward) < 1e-6f)
		{
			const vec3 reference = std::abs(character.up.y) < 0.9f ? vec3(0, 1, 0) : vec3(1, 0, 0);
			character.forward = reference - character.up * glm::dot(reference, character.up);
		}
		character.forward = glm::normalize(character.forward);
		const PointComponentSlotId character_id = world.data.createPointComponentSlot(
			point_id, PointComponentTypeId::character, &character);
		world.data.addPointComponentSlotToChunk(chunk_id, character_id);
		NpcControllerComponent controller;
		controller.character_slot_id = character_id;
		const PointComponentSlotId controller_id = world.data.createPointComponentSlot(
			point_id, PointComponentTypeId::npc_controller, &controller);
		world.data.addPointComponentSlotToChunk(chunk_id, controller_id);
		return controller_id;
	}
}
