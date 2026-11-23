#include <danikk_space_engine/systems/telemetry/telemetry.h>
#include <danikk_space_engine/world_body.h>
#include <danikk_space_engine/world.h>
#include <danikk_space_engine/point.h>
#include <danikk_space_engine/systems/human/system.h>
#include <danikk_space_engine/systems/planet/system.h>
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

	void WorldBodyData::attachPointToBody(WorldBodyId body_id, PointId point_id,
		const glm::dvec3& body_local_position)
	{
		if(!isWorldBodyActive(body_id) || !world.getPointData().isPointActive(point_id))
			danikk_engine::formatFatalError("Cannot attach an inactive point to a WorldBody");
		Point& point = world.getPointData().getPoint(point_id);
		WorldBody& body = getWorldBody(body_id);
		if(point.world_body_id != static_cast<WorldBodyId>(-1)
			&& point.world_body_id != body_id)
			danikk_engine::formatFatalError("Point % is already attached to another WorldBody", point_id);
		if(point.world_body_id == static_cast<WorldBodyId>(-1))
		{
			point.world_body_id = body_id;
			point.world_body_point_index = body.point_ids.size();
			body.point_ids.push(point_id);
		}
		if(point.world_body_id == body_id)
			body.spatial_tree.remove(point_id, point.body_local_position);
		point.body_local_position = body_local_position;
		body.spatial_tree.insert(point_id, body_local_position);
	}

	/** @copydoc WorldBodyData::destroyWorldBody */
	void WorldBodyData::destroyWorldBody(WorldBodyId id)
	{
		if(!isWorldBodyActive(id)) return;
		WorldBody& body = world_bodies[id];
		world.getHumanSystem().destroyWorldBodyHumans(id);
		while(body.point_ids.size() > 0)
			world.getPointData().destroyPoint(body.point_ids[body.point_ids.size() - 1]);
		if(body.planet_id != static_cast<PlanetId>(-1))
			world.getPlanetSystem().destroyPlanet(body.planet_id);
		if(body.detail_mesh.mesh != nullptr)
			world.getTelemetrySystem().mesh_generation_telemetry.recordMeshDeletion(
				MeshDeletionReason::world_body_destroyed);
		destroyMeshRenderData(body.detail_mesh);
		body.detail_mesh_bounds_valid = false;
		if(body.point_ids.size() > 0)
			danikk_engine::formatFatalError("World body % still owns points during destruction", id);
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
			destroyMeshRenderData(world_bodies[id].detail_mesh);
		for(WorldBodyId id : world_bodies.usedIds())
			if(world_bodies[id].point_ids.size() > 0)
				danikk_engine::formatFatalError("World body % still owns points during WorldBodyData clear", id);
		clearPools(world_body_components, world_bodies);
	}

	/** @copydoc WorldBodyData::release */
	void WorldBodyData::release()
	{
		clear();
		releasePools(world_bodies, world_body_components);
	}
}
