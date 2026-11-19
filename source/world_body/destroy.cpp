#include <danikk_space_engine/profiler.h>
#include <danikk_space_engine/world_body.h>

#include <cstdlib>

namespace danikk_space_engine
{
	namespace
	{
		/** @brief Repeatedly destroys the last active ID until the owning list is empty. */
		template<typename list_t, typename destroy_t>
		void destroyLastUntilEmpty(const list_t& ids, destroy_t destroy)
		{
			while(ids.size() > 0) destroy(ids[ids.size() - 1]);
		}

		/** @brief Clears each supplied pool while retaining its allocated capacity. */
		template<typename... pool_t>
		void clearPools(pool_t&... pools) { (pools.clear(), ...); }

		/** @brief Releases the backing storage of each supplied pool. */
		template<typename... pool_t>
		void releasePools(pool_t&... pools) { (pools.releaseMemory(), ...); }

		/** @brief Invokes a component destructor and frees external payload storage if present. */
		template<typename slot_t, typename component_type_t, typename id_t>
		void destroyComponentPayload(slot_t& slot, const component_type_t& type, id_t id, const char* component_name)
		{
			if(type.destroy == nullptr)
				danikk_engine::formatFatalError("% component type % has no destroy function", component_name, static_cast<uint32>(slot.type_id));
			if(slot.uses_external_data && slot.external_data == nullptr)
				danikk_engine::formatFatalError("% component slot % has no external data", component_name, id);
			type.destroy(slot.getData());
			if(slot.uses_external_data)
			{
				free(slot.external_data);
				slot.external_data = nullptr;
			}
		}
	}

	/** @copydoc WorldBodyData::destroyWorldBody */
	void WorldBodyData::destroyWorldBody(WorldBodyId id)
	{
		if(!isWorldBodyActive(id)) return;
		WorldBody& body = world_bodies[id];
		if(body.chunk_ids.size() > 0)
			danikk_engine::formatFatalError("World body % still owns chunks during WorldBodyData destruction", id);
		while(body.component_slot_ids.size() > 0)
			destroyWorldBodyComponentSlot(body.component_slot_ids[body.component_slot_ids.size() - 1]);
		world_bodies.free(id);
	}

	/** @copydoc WorldBodyData::destroyWorldBodyComponentSlot */
	void WorldBodyData::destroyWorldBodyComponentSlot(WorldBodyComponentSlotId id)
	{
		if(!isWorldBodyComponentSlotActive(id)) return;
		WorldBodyComponentSlot& slot = world_body_components[id];
		if(isWorldBodyActive(slot.world_body_id)) world_bodies[slot.world_body_id].removeComponentSlot(id);
		const WorldBodyComponentType& type = getWorldBodyComponentType(slot.type_id);
		destroyComponentPayload(slot, type, id, "World body");
		slot.dispose();
		world_body_components.free(id);
	}

	/** @copydoc WorldBodyData::clear */
	void WorldBodyData::clear()
	{
		destroyLastUntilEmpty(world_body_components.usedIds(),
			[this](WorldBodyComponentSlotId id) { destroyWorldBodyComponentSlot(id); });
		for(WorldBodyId id : world_bodies.usedIds())
			if(world_bodies[id].chunk_ids.size() > 0)
				danikk_engine::formatFatalError("World body % still owns chunks during WorldBodyData clear", id);
		clearPools(world_body_components, world_bodies);
	}

	/** @copydoc WorldBodyData::release */
	void WorldBodyData::release()
	{
		clear();
		releasePools(world_bodies, world_body_components);
	}
}
