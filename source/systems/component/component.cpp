#include <danikk_space_engine/profiler.h>
#include <danikk_space_engine/world.h>
#include <danikk_space_engine/systems/component/component.h>
#include <danikk_framework/log.h>

namespace danikk_space_engine
{
	void ComponentSystem::init()
	{
		danikk_framework::formatLogInfo("Init ComponentSystem");
		clearDeferredPointDestruction();
	}
	void ComponentSystem::shutdown()
	{
		danikk_framework::formatLogInfo("Shutdown ComponentSystem");
		clearDeferredPointDestruction();
	}
	/** @copydoc ComponentSystem::deferPointDestruction */
	void ComponentSystem::deferPointDestruction(PointId point_id)
	{
		DSE_WATCH();
		if(!world.point_data.isPointActive(point_id)) return;
		for(PointId queued_point_id : deferred_point_destruction_ids)
			if(queued_point_id == point_id) return;
		deferred_point_destruction_ids.push(point_id);
	}

	/** @copydoc ComponentSystem::flushDeferredPointDestruction */
	void ComponentSystem::flushDeferredPointDestruction()
	{
		DSE_WATCH();
		for(PointId point_id : deferred_point_destruction_ids)
			if(world.point_data.isPointActive(point_id)) world.chunks.destroyPoint(point_id);
		deferred_point_destruction_ids.clear();
	}

	/** @copydoc ComponentSystem::clearDeferredPointDestruction */
	void ComponentSystem::clearDeferredPointDestruction()
	{
		DSE_WATCH();
		deferred_point_destruction_ids.clear();
	}

	/** @copydoc ComponentSystem::tickPointComponents */
	void ComponentSystem::tickPointComponents()
	{
		DSE_WATCH();
		point_component_traversal_ids.clear();
		for(PointComponentSlotId slot_id : world.point_data.getUsedPointComponentSlotIds())
			point_component_traversal_ids.push(slot_id);
		for(PointComponentSlotId slot_id : point_component_traversal_ids)
		{
			if(slot_id >= world.point_data.getPointComponentSlotCount())
				danikk_engine::formatFatalError("PointData references invalid component slot %", slot_id);
			if(!world.point_data.isPointComponentSlotActive(slot_id)) continue;

			PointComponentSlot& slot = world.point_data.getPointComponentSlot(slot_id);
			if(!world.point_data.isPointActive(slot.point_id))
				danikk_engine::formatFatalError("Point component slot % references inactive point %", slot_id, slot.point_id);
			const PointComponentType& type = getPointComponentType(slot.type_id);
			if(slot.uses_external_data && slot.external_data == nullptr)
			{
				danikk_engine::formatFatalError("Point component slot % has no external data", slot_id);
			}
			if(type.tick != nullptr) type.tick(slot, world);
		}
	}

	/** @copydoc ComponentSystem::tickWorldBodyComponents */
	void ComponentSystem::tickWorldBodyComponents()
	{
		DSE_WATCH();
		const DynamicArray<WorldBodyId> world_body_ids(world.world_body_data.getUsedWorldBodyIds());
		DynamicArray<WorldBodyComponentSlotId> component_slot_ids;

		for(WorldBodyId body_id : world_body_ids)
		{
			if(!world.world_body_data.isWorldBodyActive(body_id)) continue;
			component_slot_ids.clear();
			{
				const WorldBody& body = world.world_body_data.getWorldBody(body_id);
				for(WorldBodyComponentSlotId slot_id : body.component_slot_ids)
					component_slot_ids.push(slot_id);
			}

			for(WorldBodyComponentSlotId slot_id : component_slot_ids)
			{
				if(!world.world_body_data.isWorldBodyComponentSlotActive(slot_id)) continue;
				WorldBodyComponentSlot& slot = world.world_body_data.getWorldBodyComponentSlot(slot_id);
				const WorldBodyComponentType& type = getWorldBodyComponentType(slot.type_id);
				if(type.tick != nullptr) type.tick(slot, world);
			}
		}
	}

	/** @copydoc ComponentSystem::renderWorldBodyComponents */
	void ComponentSystem::renderWorldBodyComponents()
	{
		DSE_WATCH();
		const DynamicArray<WorldBodyId> world_body_ids(world.world_body_data.getUsedWorldBodyIds());
		DynamicArray<WorldBodyComponentSlotId> component_slot_ids;
		for(WorldBodyId body_id : world_body_ids)
		{
			if(!world.world_body_data.isWorldBodyActive(body_id)) continue;
			component_slot_ids.clear();
			for(WorldBodyComponentSlotId slot_id : world.world_body_data.getWorldBody(body_id).component_slot_ids)
				component_slot_ids.push(slot_id);

			for(WorldBodyComponentSlotId slot_id : component_slot_ids)
			{
				if(!world.world_body_data.isWorldBodyComponentSlotActive(slot_id)) continue;
				WorldBodyComponentSlot& slot = world.world_body_data.getWorldBodyComponentSlot(slot_id);
				const WorldBodyComponentType& type = getWorldBodyComponentType(slot.type_id);
				if(type.frame != nullptr) type.frame(slot, world);
			}
		}
	}

	/** @copydoc ComponentSystem::renderPointComponents */
	void ComponentSystem::renderPointComponents()
	{
		DSE_WATCH();
		point_component_traversal_ids.clear();
		for(PointComponentSlotId slot_id : world.point_data.getUsedPointComponentSlotIds())
			point_component_traversal_ids.push(slot_id);
		for(PointComponentSlotId slot_id : point_component_traversal_ids)
		{
			if(slot_id >= world.point_data.getPointComponentSlotCount())
				danikk_engine::formatFatalError("PointData references invalid component slot %", slot_id);
			if(!world.point_data.isPointComponentSlotActive(slot_id)) continue;
			PointComponentSlot& slot = world.point_data.getPointComponentSlot(slot_id);
			if(!world.point_data.isPointActive(slot.point_id))
				danikk_engine::formatFatalError("Point component slot % references inactive point %", slot_id, slot.point_id);
			if(slot.uses_external_data && slot.external_data == nullptr)
				danikk_engine::formatFatalError("Point component slot % has no external data", slot_id);
			const PointComponentType& type = getPointComponentType(slot.type_id);
			if(type.frame != nullptr) type.frame(slot, world);
		}
	}
}
