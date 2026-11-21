#include <cmath>

#include <danikk_space_engine/systems/telemetry/telemetry.h>
#include <danikk_space_engine/component/point/human/human.h>
#include <danikk_space_engine/assets/human_skin.h>
#include <danikk_space_engine/component/point/damageable.h>
#include <danikk_space_engine/component/point/inventory.h>
#include <danikk_space_engine/item/item.h>
#include <danikk_space_engine/systems/unit/unit.h>
#include <danikk_space_engine/world.h>

namespace danikk_space_engine
{
	/**
	 * @brief Creates a soldier on a WorldBody with a skin, equipped MG-42, and AI registration.
	 *
	 * The input position is the world-body-local location of the feet. The point is placed half a
	 * human height along `up`, relative to the existing owning body.
	 * The human and inventory slots share the same point. UnitSystem owns AI state.
	 * @param world World receiving the point and component slots.
	 * @param body_id Active WorldBody receiving the NPC.
	 * @param position Feet position in world-body-local coordinates.
	 * @param up Local vertical direction; normalized, with +Z used if the input is degenerate.
	 * @param forward Initial facing direction, projected onto the plane perpendicular to `up`.
	 * @param faction_id Faction whose UnitSystem will own this unit's AI state.
	 * @return ID of the attached human component slot.
	 */
	PointComponentSlotId createHumanSoldier(World& world, WorldBodyId body_id,
		const glm::dvec3& position, const vec3& up, const vec3& forward, FactionId faction_id)
	{
		DSE_WATCH();
		if(!world.world_body_data.isWorldBodyActive(body_id)) formatFatalError("Cannot create NPC on an invalid WorldBody");
		const PointId point_id = world.point_data.createPoint();
		HumanComponent human;
		human.faction_id = faction_id;
		human.appearance.skin_id = human_skin_ids::usmc_marpat_woodland;
		human.up = glm::length(up) > 1e-6f ? glm::normalize(up) : vec3(0, 0, 1);
		human.setInitialForward(forward);
		const glm::dvec3 human_position = position + glm::dvec3(human.up * (human.getHeight() * 0.5f));
		const PointComponentSlotId human_id = world.point_data.createPointComponentSlot(
			point_id, PointComponentTypeId::human, &human);
		world.attachPointComponentToBody(body_id, point_id, human_id, human_position);
		DamageableComponent damageable;
		damageable.setHealth(getDamageableDefinition(damageable.definition_id).max_health, point_id);
		const PointComponentSlotId damageable_id = world.point_data.createPointComponentSlot(
			point_id, PointComponentTypeId::damageable, &damageable);
		world.attachPointComponentToBody(body_id, point_id, damageable_id, human_position);

		InventoryComponent inventory;
		inventory.slots.pushCtor();
		InventorySlot& weapon_slot = inventory.slots[0];
		weapon_slot.item_definition_id = item_definition_ids::mg42;
		weapon_slot.instance_state.weapon = WeaponItemState{};
		weapon_slot.count = 1;
		weapon_slot.is_equipped = true;
		const PointComponentSlotId inventory_id = world.point_data.createPointComponentSlot(
			point_id, PointComponentTypeId::inventory, &inventory);
		world.attachPointComponentToBody(body_id, point_id, inventory_id, human_position);
		if(!equipItem(world, point_id, weapon_slot.item_definition_id))
			formatFatalError("Failed to equip MG-42 for human soldier");

		world.unit_system.registerAgent(human_id);
		return human_id;
	}

	/**
	 * @brief Creates the current default NPC archetype by delegating to createHumanSoldier().
	 * @param world World receiving the NPC resources.
	 * @param body_id Active WorldBody receiving the NPC.
	 * @param position Feet position in body-local coordinates.
	 * @param up Initial local up direction.
	 * @param forward Initial local facing direction.
	 * @param faction_id Faction whose UnitSystem receives the new unit.
	 * @return ID of the new human component slot.
	 */
	PointComponentSlotId createNpc(World& world, WorldBodyId body_id,
		const glm::dvec3& position, const vec3& up, const vec3& forward, FactionId faction_id)
	{
		DSE_WATCH();
		return createHumanSoldier(world, body_id, position, up, forward, faction_id);
	}
}
