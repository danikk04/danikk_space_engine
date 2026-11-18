#include <cmath>

#include <danikk_space_engine/profiler.h>
#include <danikk_space_engine/component/point/human/npc_controller.h>
#include <danikk_space_engine/component/point/human/human.h>
#include <danikk_space_engine/component/point/human/skin.h>
#include <danikk_space_engine/component/point/damageable.h>
#include <danikk_space_engine/component/point/inventory.h>
#include <danikk_space_engine/item/item.h>
#include <danikk_space_engine/world/world.h>

namespace danikk_space_engine
{
	/**
	 * @brief Creates a soldier in a standalone chunk with a skin, equipped MG-42, and NPC controller.
	 *
	 * The input position is the world-body-local location of the feet. The point is placed half a
	 * human height along `up`, relative to the existing owning chunk.
	 * The human, inventory, and controller slots share the same point and chunk.
	 * @param world World receiving the new chunk, point, and component slots.
	 * @param chunk_id Active chunk receiving the NPC components.
	 * @param position Feet position in world-body-local coordinates.
	 * @param up Local vertical direction; normalized, with +Z used if the input is degenerate.
	 * @param forward Initial facing direction, projected onto the plane perpendicular to `up`.
	 * @return ID of the attached NPC controller slot.
	 */
	PointComponentSlotId createHumanSoldier(World& world, ChunkId chunk_id,
		const glm::dvec3& position, const vec3& up, const vec3& forward)
	{
		DSE_WATCH();
		if(!world.data.isChunkActive(chunk_id)) formatFatalError("Cannot create NPC in an invalid chunk");
		const glm::dvec3 chunk_position = world.data.getChunk(chunk_id).position;
		const PointId point_id = world.data.createPoint();
		Point& point = world.data.getPoint(point_id);
		HumanComponent human;
		human.faction_id = base_faction_ids::player;
		human.appearance.skin_id = human_skin_ids::usmc_marpat_woodland;
		human.up = glm::length(up) > 1e-6f ? glm::normalize(up) : vec3(0, 0, 1);
		human.setInitialForward(forward);
		point.position = vec3(position - chunk_position) + human.up * (human.getHeight() * 0.5f);
		const PointComponentSlotId human_id = world.data.createPointComponentSlot(
			point_id, PointComponentTypeId::human, &human);
		world.data.addPointComponentSlotToChunk(chunk_id, human_id);
		DamageableComponent damageable;
		damageable.setHealth(getDamageableDefinition(damageable.definition_id).max_health, point_id);
		const PointComponentSlotId damageable_id = world.data.createPointComponentSlot(
			point_id, PointComponentTypeId::damageable, &damageable);
		world.data.addPointComponentSlotToChunk(chunk_id, damageable_id);

		InventoryComponent inventory;
		inventory.slots.pushCtor();
		InventorySlot& weapon_slot = inventory.slots[0];
		weapon_slot.item_definition_id = item_definition_ids::mg42;
		weapon_slot.instance_state.weapon = WeaponItemState{};
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

	/**
	 * @brief Creates the current default NPC archetype by delegating to createHumanSoldier().
	 * @param world World receiving the NPC resources.
	 * @param chunk_id Active chunk receiving the NPC.
	 * @param position Feet position in body-local coordinates.
	 * @param up Initial local up direction.
	 * @param forward Initial local facing direction.
	 * @return ID of the NPC controller slot created with the human.
	 */
	PointComponentSlotId createNpc(World& world, ChunkId chunk_id,
		const glm::dvec3& position, const vec3& up, const vec3& forward)
	{
		DSE_WATCH();
		return createHumanSoldier(world, chunk_id, position, up, forward);
	}
}
