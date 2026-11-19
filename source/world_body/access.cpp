#include <danikk_space_engine/world_body.h>

namespace danikk_space_engine
{
	namespace
	{
		/** @brief Checks a world-body-pool ID and returns the active resource. */
		template<typename resource_t, typename id_t>
		resource_t& getCheckedResource(WorldResourcePool<resource_t, id_t>& pool, id_t id, const char* resource_name)
		{
			if(!pool.isActive(id)) danikk_engine::formatFatalError("Invalid % ID %", resource_name, id);
			return pool[id];
		}

		/** @brief Checks a world-body-pool ID and returns the active resource as const. */
		template<typename resource_t, typename id_t>
		const resource_t& getCheckedResource(
			const WorldResourcePool<resource_t, id_t>& pool,
			id_t id,
			const char* resource_name)
		{
			if(!pool.isActive(id)) danikk_engine::formatFatalError("Invalid % ID %", resource_name, id);
			return pool[id];
		}
	}

	/** @copydoc WorldBodyData::isWorldBodyActive */
	bool WorldBodyData::isWorldBodyActive(WorldBodyId id) const { return world_bodies.isActive(id); }
	/** @copydoc WorldBodyData::isWorldBodyComponentSlotActive */
	bool WorldBodyData::isWorldBodyComponentSlotActive(WorldBodyComponentSlotId id) const { return world_body_components.isActive(id); }
	/** @copydoc WorldBodyData::getUsedWorldBodyIds */
	const DynamicArray<WorldBodyId>& WorldBodyData::getUsedWorldBodyIds() const { return world_bodies.usedIds(); }
	/** @copydoc WorldBodyData::getUsedWorldBodyComponentSlotIds */
	const DynamicArray<WorldBodyComponentSlotId>& WorldBodyData::getUsedWorldBodyComponentSlotIds() const { return world_body_components.usedIds(); }
	/** @copydoc WorldBodyData::getWorldBody */
	WorldBody& WorldBodyData::getWorldBody(WorldBodyId id) { return getCheckedResource(world_bodies, id, "world body"); }
	/** @copydoc WorldBodyData::getWorldBody */
	const WorldBody& WorldBodyData::getWorldBody(WorldBodyId id) const { return getCheckedResource(world_bodies, id, "world body"); }
	/** @copydoc WorldBodyData::getWorldBodyComponentSlot */
	WorldBodyComponentSlot& WorldBodyData::getWorldBodyComponentSlot(WorldBodyComponentSlotId id) { return getCheckedResource(world_body_components, id, "world body component slot"); }
	/** @copydoc WorldBodyData::getWorldBodyComponentSlot */
	const WorldBodyComponentSlot& WorldBodyData::getWorldBodyComponentSlot(WorldBodyComponentSlotId id) const { return getCheckedResource(world_body_components, id, "world body component slot"); }
	/** @copydoc WorldBodyData::getWorldBodySlotCount */
	size_t WorldBodyData::getWorldBodySlotCount() const { return world_bodies.size(); }
	/** @copydoc WorldBodyData::getWorldBodyComponentSlotCount */
	size_t WorldBodyData::getWorldBodyComponentSlotCount() const { return world_body_components.size(); }
}
