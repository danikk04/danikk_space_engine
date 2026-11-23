#include <cmath>
#include <danikk_space_engine/systems/world_body/system.h>

#include <danikk_space_engine/systems/telemetry/telemetry.h>
#include <danikk_space_engine/assets/human_skin.h>
#include <danikk_space_engine/systems/human/damageable.h>
#include <danikk_space_engine/systems/human/inventory.h>
#include <danikk_space_engine/systems/human/system.h>
#include <danikk_space_engine/item/item.h>
#include <danikk_space_engine/systems/unit/unit.h>
#include <danikk_space_engine/world.h>

#include <danikk_space_engine/systems/world_body/world_body.h>

namespace danikk_space_engine
{
	/** @brief Creates and registers a faction-controlled soldier with the default loadout. */
	HumanId HumanSystem::createNpc(WorldBodyId body_id, const glm::dvec3& feet_position,
		const vec3& up, const vec3& forward, FactionId faction_id)
	{
		DSE_WATCH();
		if(!world.getWorldBodySystem().isWorldBodyActive(body_id))
			formatFatalError("Cannot create NPC on an invalid WorldBody");

		HumanState initial_state;
		initial_state.faction_id = faction_id;
		initial_state.appearance.skin_id = human_skin_ids::usmc_marpat_woodland;
		initial_state.up = glm::length(up) > 1e-6f ? glm::normalize(up) : vec3(0, 0, 1);
		initial_state.setInitialForward(forward);
		const glm::dvec3 body_local_position = feet_position
			+ glm::dvec3(initial_state.up * (initial_state.getHeight() * 0.5f));
		HumanData& human = createHuman(body_id, body_local_position, initial_state);
		human.damageable.setHealth(getDamageableDefinition(
			human.damageable.definition_id).max_health);
		human.inventory.slots.pushCtor();
		InventorySlot& weapon_slot = human.inventory.slots[0];
		weapon_slot.item_definition_id = item_definition_ids::mg42;
		weapon_slot.instance_state.weapon = WeaponItemState{};
		weapon_slot.count = 1;
		weapon_slot.is_equipped = true;
		human.arm_pose = HumanArmPose::weapon_ready;

		world.getUnitSystem().registerAgent(human.id);
		return human.id;
	}
}
