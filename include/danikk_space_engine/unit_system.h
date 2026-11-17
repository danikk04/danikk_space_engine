#pragma once

#include <danikk_space_engine/id.h>
#include <danikk_framework/dynamic_array.h>

namespace danikk_space_engine
{
	class World;

	/** @brief One faction's report that a hostile unit was seen. */
	struct UnitSighting
	{
		FactionId faction_id = static_cast<FactionId>(-1); ///< Faction receiving this shared intelligence.
		PointComponentSlotId target_slot_id = static_cast<PointComponentSlotId>(-1); ///< Observed human slot.
		PointId target_point_id = static_cast<PointId>(-1); ///< Observed point identity.
		uint64 target_generation_id = 0; ///< Slot incarnation used to reject recycled IDs.
		glm::dvec3 position = glm::dvec3(0.0); ///< Last world position at which the target was visible.
		float age = 0.0f; ///< Seconds since any allied unit last refreshed this contact.
	};

	/**
	 * @brief Performs NPC perception and assigns executable orders to units.
	 *
	 * UnitSystem owns target scanning, visibility raycasts, faction intelligence, and tactical choices.
	 * NPC controller callbacks only execute movement, look, equipment, and firing commands written here.
	 */
	class UnitSystem
	{
		/** @brief Recent faction-wide contacts, aged and pruned on each world tick. */
		DynamicArray<UnitSighting> known_sightings;

	public:
		/** @brief Performs perception and raycasts, shares sightings, then refreshes all NPC orders. */
		void tick(World& world, float dt);

		/** @brief Clears remembered faction contacts. */
		void clear();
	};
}
