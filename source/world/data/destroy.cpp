#include <danikk_space_engine/profiler.h>
#include <danikk_space_engine/point.h>
#include <danikk_space_engine/world_body.h>
#include <danikk_space_engine/component/point/build_part.h>

#include <cstdlib>

namespace danikk_space_engine
{
	namespace
	{


		/**
		 * @brief Iterates active resources and invokes the destroy callback for predicate matches.
		 * @tparam pool_t Resource pool type.
		 * @tparam predicate_t Predicate accepting a resource reference.
		 * @tparam destroy_t Callback accepting the matching resource ID.
		 * @param pool Pool to traverse; traversal tolerates swap removal by rechecking the moved ID.
		 * @param matches Predicate deciding whether to destroy each resource.
		 * @param destroy Normal destruction callback for a matched ID.
		 */
		template<typename pool_t, typename predicate_t, typename destroy_t>
		void destroyMatchingResources(pool_t& pool, predicate_t matches, destroy_t destroy)
		{
			for(size_t index = 0; index < pool.usedIds().size();)
			{
				const auto id = pool.usedIds()[index];
				if(matches(pool[id])) destroy(id);
				else index++;
			}
		}

		/**
		 * @brief Repeatedly destroys the last ID until an owner list is empty.
		 * @tparam list_t Array-like owner list exposing size() and indexed access.
		 * @tparam destroy_t Callback that removes the supplied resource ID from the list.
		 * @param ids List that is shortened by the callback.
		 * @param destroy Resource destruction callback.
		 */
		template<typename list_t, typename destroy_t>
		void destroyLastUntilEmpty(const list_t& ids, destroy_t destroy)
		{
			while(ids.size() > 0) destroy(ids[ids.size() - 1]);
		}

		/**
		 * @brief Calls clear() on every supplied pool, destroying elements but retaining array capacity.
		 * @tparam pool_t Pool types accepted by the fold expression.
		 * @param pools Pools to clear.
		 */
		template<typename... pool_t>
		void clearPools(pool_t&... pools)
		{
			(pools.clear(), ...);
		}

		/**
		 * @brief Calls releaseMemory() on every supplied pool to return backing arrays to the allocator.
		 * @tparam pool_t Pool types accepted by the fold expression.
		 * @param pools Pools whose storage is released.
		 */
		template<typename... pool_t>
		void releasePools(pool_t&... pools)
		{
			(pools.releaseMemory(), ...);
		}

		/**
		 * @brief Runs the registered component destructor and frees external payload storage when used.
		 * @tparam slot_t Point or world-body component slot type.
		 * @tparam component_type_t Matching callback descriptor type.
		 * @tparam id_t Slot ID type used for diagnostic messages.
		 * @param slot Component slot whose payload is being destroyed.
		 * @param type Descriptor supplying the destroy callback.
		 * @param id Slot ID reported if required storage/callbacks are missing.
		 * @param component_name Human-readable category used in fatal diagnostics.
		 */
		template<typename slot_t, typename component_type_t, typename id_t>
		void destroyComponentPayload(
			slot_t& slot,
			const component_type_t& type,
			id_t id,
			const char* component_name)
		{
			if(type.destroy == nullptr)
			{
				danikk_engine::formatFatalError("% component type % has no destroy function", component_name, static_cast<uint32>(slot.type_id));
			}
			if(slot.uses_external_data && slot.external_data == nullptr)
			{
				danikk_engine::formatFatalError("% component slot % has no external data", component_name, id);
			}
			type.destroy(slot.getData());
			if(slot.uses_external_data)
			{
				free(slot.external_data);
				slot.external_data = nullptr;
			}
		}
	}

	/**
	 * @brief Destroys all point components that reference the point.
	 *
	 * Destroys dependent resources through their regular cleanup paths, then recycles
	 * the point slot. Chunk links must already be detached by ChunkSystem. The
	 * pool scan keeps its index after a removal because swap-remove may move another
	 * active ID into that position.
	 * @param id Point ID to destroy; inactive IDs are ignored.
	 */
	void PointData::destroyPoint(PointId id)
	{
		/**
		 * @brief Measures the duration of the enclosing profiler scope.
		 */
		DSE_WATCH();
		if(!isPointActive(id)) return;


		destroyMatchingResources(point_components,
			[id](const PointComponentSlot& slot) { return slot.point_id == id; },
			[this](PointComponentSlotId slot_id) { destroyPointComponentSlot(slot_id); });
		points.free(id);
	}


	/**
	 * @brief Destroys all components owned by the body before returning its slot to the pool.
	 * @param id WorldBody ID to destroy; inactive IDs are ignored.
	 */
	void WorldBodyData::destroyWorldBody(WorldBodyId id)
	{
		if(!isWorldBodyActive(id)) return;
		WorldBody& body = world_bodies[id];
		if(body.chunk_ids.size() > 0)
			danikk_engine::formatFatalError("World body % still owns chunks during WorldBodyData destruction", id);

		while(body.component_slot_ids.size() > 0)
		{
			destroyWorldBodyComponentSlot(body.component_slot_ids[body.component_slot_ids.size() - 1]);
		}
		world_bodies.free(id);
	}

	/**
	 * @brief Detaches a body component, destroys its payload, resets the slot, and recycles its ID.
	 * @param id Component slot ID to destroy; inactive IDs are ignored.
	 */
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

	/**
	 * @brief Releases a point component payload and slot after ChunkSystem detaches its membership.
	 * @param id Component slot ID to destroy; inactive IDs are ignored.
	 */
	void PointData::destroyPointComponentSlot(PointComponentSlotId id)
	{
		if(!isPointComponentSlotActive(id)) return;
		const PointComponentSlot& destroyed_slot = point_components[id];
		if(destroyed_slot.type_id == PointComponentTypeId::build_part)
		{
			for(PointComponentSlotId other_id : point_components.usedIds())
				if(other_id != id && isPointComponentSlotActive(other_id))
				{
					PointComponentSlot& other_slot = point_components[other_id];
					if(other_slot.type_id != PointComponentTypeId::build_part) continue;
					BuildPartComponent& child = *static_cast<BuildPartComponent*>(other_slot.getData());
					if(child.parent_slot_id == id) child.parent_slot_id = no_build_part_parent;
				}
		}
		PointComponentSlot& slot = point_components[id];
		const PointComponentType& type = getPointComponentType(slot.type_id);
		destroyComponentPayload(slot, type, id, "Point");
		slot.dispose();
		point_components.free(id);
	}


	/** @copydoc PointData::clear */
	void PointData::clear()
	{
		destroyLastUntilEmpty(point_components.usedIds(),
			[this](PointComponentSlotId id) { destroyPointComponentSlot(id); });
		clearPools(point_components, points);
	}

	/** @copydoc PointData::release */
	void PointData::release()
	{
		clear();
		releasePools(points, point_components);
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
