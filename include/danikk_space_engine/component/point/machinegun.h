#pragma once

#include <danikk_space_engine/component/point/point.h>

namespace danikk_space_engine
{

	/**
	 * @brief Stores a machine gun's local orientation and firing interval state.
	 *
	 * The component is attached to a Point. Its forward/up/right basis is in the owning WorldBody's
	 * local coordinates. Firing accumulates simulation time and creates one projectile for each
	 * elapsed 0.25-second interval; rendering resolves the shared MG-42 mesh through the item catalog.
	 */
	struct MachinegunComponent
	{
		/** @brief Uniform scale applied to the shared weapon mesh and muzzle offsets. */
		float scale = 0.25f;
		/** @brief Weapon barrel axis and bullet travel direction, in the owning body's local frame. */
		vec3 forward = vec3(1.0f, 0.0f, 0.0f);
		/** @brief Weapon's local vertical axis. */
		vec3 up = vec3(0.0f, 1.0f, 0.0f);
		/** @brief Weapon's local lateral axis. */
		vec3 right = vec3(0.0f, 0.0f, 1.0f);
		/** @brief Remainder toward the next 0.25-second firing interval, in seconds. */
		float shot_timer = 0.0f;
	};

	/**
	 * @brief Placement-constructs a machine gun component, copying optional initial orientation.
	 * @param destination Address of storage supplied by the point-component slot.
	 * @param init_data Optional pointer to a `MachinegunComponent`; null selects defaults.
	 */
	void constructMachinegunComponent(void* destination, const void* init_data);

	/**
	 * @brief Move-constructs a machine gun component during slot relocation.
	 * @param destination Address of replacement storage.
	 * @param source Existing payload, whose destructor is run after the move.
	 */
	void moveMachinegunComponent(void* destination, void* source);

	/**
	 * @brief Runs the machine gun component destructor without freeing slot-owned storage.
	 * @param data Address of the live payload.
	 */
	void destroyMachinegunComponent(void* data);

	/**
	 * @brief Advances the firing timer and spawns one bullet point per elapsed firing interval.
	 *
	 * Bullets inherit the gun's forward direction and are inserted into the same owning body.
	 * The muzzle offset is transformed by the component's orientation and scale.
	 * @param slot Slot containing the weapon payload and attached point ID.
	 * @param world World used to resolve point/body ownership and create bullet components.
	 */
	void tickMachinegunComponent(PointComponentSlot& slot, World& world);

	/**
	 * @brief Draws the catalogued MG-42 mesh at the attached point using the stored orientation.
	 * @param slot Slot containing the weapon payload, point ID, and owning body ID.
	 * @param world World used to build the body's world transform.
	 */
	void frameMachinegunComponent(PointComponentSlot& slot, World& world);
}
