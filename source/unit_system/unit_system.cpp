#include <danikk_space_engine/unit_system.h>
#include <danikk_space_engine/world/world.h>

#include "internal.h"

namespace danikk_space_engine
{
	namespace
	{
		/** @brief Holds one independent tactical state object for each faction index. */
		DynamicArray<UnitSystem> unit_systems;
	}

	/** @brief Updates each faction's perception and orders for the current world tick. */
	void unitSystemTick(World& world, float dt)
	{
		const size_t faction_count = world.data.factions.size();
		while(unit_systems.size() > faction_count)
			unit_systems.resize(unit_systems.size() - 1);
		while(unit_systems.size() < faction_count)
		{
			UnitSystem system;
			system.faction_id = static_cast<FactionId>(unit_systems.size());
			unit_systems.push(static_cast<UnitSystem&&>(system));
		}

		const DynamicArray<PointComponentSlotId>& slot_ids = world.data.getUsedPointComponentSlotIds();
		for(UnitSystem& system : unit_systems)
		{
			unit_system_internal::updatePerception(world, system, slot_ids, dt);
			unit_system_internal::assignOrders(world, system, slot_ids);
		}
	}

	namespace unit_system_internal
	{
		/** @brief Releases all faction-system state when the world is reset or shut down. */
		void resetUnitSystems()
		{
			unit_systems.clear();
		}
	}
}
