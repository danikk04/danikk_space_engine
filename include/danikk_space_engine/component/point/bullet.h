#pragma once

#include <danikk_space_engine/component/point/point.h>

namespace danikk_space_engine
{

	/**
	 * @brief Stores the motion and lifetime of a projectile.
	 */
	struct BulletComponent
	{
		/** @brief Projectile velocity in the owning chunk's local frame, measured in units per second. */
		vec3 velocity = vec3(0.0f);
		/** @brief Simulation time elapsed since this projectile was spawned. */
		float age = 0.0f;
		/** @brief Maximum projectile lifetime before its point is queued for destruction. */
		float lifetime = 5.0f;
		/** @brief Prevents duplicate deferred-destruction requests after impact or timeout. */
		bool destruction_queued = false;
	};

	/**
	 * @brief Placement-constructs a bullet, copying optional initial velocity and lifetime.
	 * @param destination Address of storage supplied by the point-component slot.
	 * @param init_data Optional pointer to a `BulletComponent`; null selects defaults.
	 */
	void constructBulletComponent(void* destination, const void* init_data);

	/**
	 * @brief Move-constructs a bullet payload during slot relocation.
	 * @param destination Address of replacement payload storage.
	 * @param source Existing payload, whose destructor is run after the move.
	 */
	void moveBulletComponent(void* destination, void* source);

	/**
	 * @brief Runs the bullet payload destructor without freeing slot-owned storage.
	 * @param data Address of the live bullet payload.
	 */
	void destroyBulletComponent(void* data);

	/**
	 * @brief Sweeps the projectile's traveled segment for collision, advances age, and queues removal.
	 *
	 * The segment raycast covers the full per-tick displacement to avoid tunneling. On impact,
	 * the point is moved to the hit position and deferred for destruction; expired bullets are
	 * deferred in the same way.
	 * @param slot Slot containing the projectile payload and attached point/chunk IDs.
	 * @param world World used for world-space conversion, raycasting, and deferred cleanup.
	 */
	void tickBulletComponent(PointComponentSlot& slot, World& world);

	/**
	 * @brief Draws the projectile as an elongated bright marker aligned with its velocity.
	 * @param slot Slot containing the projectile payload and attached point/chunk IDs.
	 * @param world World used to transform the local point into render coordinates.
	 */
	void frameBulletComponent(PointComponentSlot& slot, World& world);
}
