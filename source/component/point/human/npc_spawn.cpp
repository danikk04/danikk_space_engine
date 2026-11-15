#include <cmath>

#include <danikk_space_engine/component/point/human/npc_controller.h>
#include <danikk_space_engine/component/point/human/human.h>
#include <danikk_space_engine/component/point/inventory.h>
#include <danikk_space_engine/item.h>
#include <danikk_space_engine/asset.h>
#include <danikk_space_engine/world/world.h>

namespace danikk_space_engine
{
	// Creates an NPC in a separate component chunk so unloading generated terrain does not delete it.
	// Input position/orientation are body-local. Returns the controller slot for later commands.
	PointComponentSlotId createHumanSoldier(World& world, WorldBodyId world_body_id,
		const glm::dvec3& position, const vec3& up, const vec3& forward)
	{
		if(!world.data.isWorldBodyActive(world_body_id)) formatFatalError("Cannot create NPC on an invalid body");
		Chunk& chunk = world.data.createChunk(world_body_id);
		chunk.position = position;
		const ChunkId chunk_id = chunk.chunk_id;
		const PointId point_id = world.data.createPoint();
		HumanComponent human;
		human.appearance.skin_id = static_asset_collection.usmc_marpat_woodland_skin_id;
		human.up = glm::length(up) > 1e-6f ? glm::normalize(up) : vec3(0, 0, 1);
		human.forward = forward - human.up * glm::dot(forward, human.up);
		if(glm::length(human.forward) < 1e-6f)
		{
			const vec3 reference = std::abs(human.up.y) < 0.9f ? vec3(0, 1, 0) : vec3(1, 0, 0);
			human.forward = reference - human.up * glm::dot(reference, human.up);
		}
		human.forward = glm::normalize(human.forward);
		const PointComponentSlotId human_id = world.data.createPointComponentSlot(
			point_id, PointComponentTypeId::human, &human);
		world.data.addPointComponentSlotToChunk(chunk_id, human_id);

		InventoryComponent inventory;
		inventory.slots.pushCtor();
		InventorySlot& weapon_slot = inventory.slots[0];
		weapon_slot.item_definition_id = item_definition_ids::mg42;
		weapon_slot.count = 1;
		weapon_slot.is_equipped = true;
		const PointComponentSlotId inventory_id = world.data.createPointComponentSlot(
			point_id, PointComponentTypeId::inventory, &inventory);
		world.data.addPointComponentSlotToChunk(chunk_id, inventory_id);
		if(!equipItem(world, point_id, weapon_slot.item_definition_id))
			formatFatalError("Failed to equip MG-42 for human soldier");

		NpcControllerComponent controller;
		controller.human_slot_id = human_id;
		const PointComponentSlotId controller_id = world.data.createPointComponentSlot(
			point_id, PointComponentTypeId::npc_controller, &controller);
		world.data.addPointComponentSlotToChunk(chunk_id, controller_id);
		return controller_id;
	}

	PointComponentSlotId createNpc(World& world, WorldBodyId world_body_id,
		const glm::dvec3& position, const vec3& up, const vec3& forward)
	{
		return createHumanSoldier(world, world_body_id, position, up, forward);
	}
}
