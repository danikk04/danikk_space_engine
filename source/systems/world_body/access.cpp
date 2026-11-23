#include <danikk_space_engine/systems/world_body/system.h>

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

	/** @copydoc WorldBodySystem::isWorldBodyActive */
	bool WorldBodySystem::isWorldBodyActive(WorldBodyId id) const { return world_bodies.isActive(id); }
	/** @copydoc WorldBodySystem::getUsedWorldBodyIds */
	const DynamicArray<WorldBodyId>& WorldBodySystem::getUsedWorldBodyIds() const { return world_bodies.usedIds(); }
	/** @copydoc WorldBodySystem::getWorldBody */
	WorldBody& WorldBodySystem::getWorldBody(WorldBodyId id) { return getCheckedResource(world_bodies, id, "world body"); }
	/** @copydoc WorldBodySystem::getWorldBody */
	const WorldBody& WorldBodySystem::getWorldBody(WorldBodyId id) const { return getCheckedResource(world_bodies, id, "world body"); }
	/** @copydoc WorldBodySystem::getWorldBodySlotCount */
	size_t WorldBodySystem::getWorldBodySlotCount() const { return world_bodies.size(); }
}
