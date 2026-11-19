#include <cmath>

#include <danikk_space_engine/profiler.h>
#include <danikk_space_engine/component/point/human/human.h>
#include <danikk_space_engine/component/point/human/skin.h>
#include <danikk_space_engine/component/point/damageable.h>
#include <danikk_space_engine/component/point/inventory.h>
#include <danikk_space_engine/item/item.h>
#include <danikk_space_engine/unit_system.h>
#include <danikk_space_engine/world/world.h>

namespace danikk_space_engine
{
	/**
	 * @brief Creates a soldier in a chunk with a skin, equipped MG-42, and central AI registration.
	 *
	 * The input position is the world-body-local location of the feet. The point is placed half a
	 * human height along `up`, relative to the existing owning chunk.
	 * The human and inventory slots share the same point and chunk. UnitSystem owns AI state.
	 * @param world World receiving the new chunk, point, and component slots.
	 * @param chunk_id Active chunk receiving the NPC components.
	 * @param position Feet position in world-body-local coordinates.
	 * @param up Local vertical direction; normalized, with +Z used if the input is degenerate.
	 * @param forward Initial facing direction, projected onto the plane perpendicular to `up`.
	 * @param faction_id Faction whose UnitSystem will own this unit's AI state.
	 * @return ID of the attached human component slot.
	 */
	PointComponentSlotId createHumanSoldier(World& world, ChunkId chunk_id,
		const glm::dvec3& position, const vec3& up, const vec3& forward, FactionId faction_id)
	{
		DSE_WATCH();
		if(!world.chunk_system.isChunkActive(chunk_id)) formatFatalError("Cannot create NPC in an invalid chunk");
		const glm::dvec3 chunk_position = world.chunk_system.getChunk(chunk_id).position;
		const PointId point_id = world.data.createPoint();
		Point& point = world.data.getPoint(point_id);
		HumanComponent human;
		human.faction_id = faction_id;
		human.appearance.skin_id = human_skin_ids::usmc_marpat_woodland;
		human.up = glm::length(up) > 1e-6f ? glm::normalize(up) : vec3(0, 0, 1);
		human.setInitialForward(forward);
		point.position = vec3(position - chunk_position) + human.up * (human.getHeight() * 0.5f);
		const PointComponentSlotId human_id = world.data.createPointComponentSlot(
			point_id, PointComponentTypeId::human, &human);
		world.chunk_system.addPointComponentSlotToChunk(world, chunk_id, human_id);
		DamageableComponent damageable;
		damageable.setHealth(getDamageableDefinition(damageable.definition_id).max_health, point_id);
		const PointComponentSlotId damageable_id = world.data.createPointComponentSlot(
			point_id, PointComponentTypeId::damageable, &damageable);
		world.chunk_system.addPointComponentSlotToChunk(world, chunk_id, damageable_id);

		InventoryComponent inventory;
		inventory.slots.pushCtor();
		InventorySlot& weapon_slot = inventory.slots[0];
		weapon_slot.item_definition_id = item_definition_ids::mg42;
		weapon_slot.instance_state.weapon = WeaponItemState{};
		weapon_slot.count = 1;
		weapon_slot.is_equipped = true;
		const PointComponentSlotId inventory_id = world.data.createPointComponentSlot(
			point_id, PointComponentTypeId::inventory, &inventory);
		world.chunk_system.addPointComponentSlotToChunk(world, chunk_id, inventory_id);
		if(!equipItem(world, point_id, weapon_slot.item_definition_id))
			formatFatalError("Failed to equip MG-42 for human soldier");

		registerUnitAgent(world, human_id);
		return human_id;
	}

	/**
	 * @brief Creates the current default NPC archetype by delegating to createHumanSoldier().
	 * @param world World receiving the NPC resources.
	 * @param chunk_id Active chunk receiving the NPC.
	 * @param position Feet position in body-local coordinates.
	 * @param up Initial local up direction.
	 * @param forward Initial local facing direction.
	 * @param faction_id Faction whose UnitSystem receives the new unit.
	 * @return ID of the new human component slot.
	 */
	PointComponentSlotId createNpc(World& world, ChunkId chunk_id,
		const glm::dvec3& position, const vec3& up, const vec3& forward, FactionId faction_id)
	{
		DSE_WATCH();
		return createHumanSoldier(world, chunk_id, position, up, forward, faction_id);
	}
}
