#include <danikk_space_engine/world_body.h>

namespace danikk_space_engine
{
	namespace
	{
		/** @brief Rejects invalid world-body component registrations before allocation. */
		template<typename component_type_t, typename type_id_t>
		void validateComponentType(const component_type_t& type, type_id_t type_id, const char* component_name)
		{
			if(static_cast<uint32>(type_id) == 0 || type.construct == nullptr || type.data_size == 0)
				danikk_engine::formatFatalError("% component type % cannot be constructed", component_name, static_cast<uint32>(type_id));
			if(type.data_alignment > alignof(std::max_align_t))
				danikk_engine::formatFatalError("% component type % requires unsupported alignment", component_name, static_cast<uint32>(type_id));
		}

		/** @brief Allocates component payload storage and invokes its registered constructor. */
		template<typename slot_t, typename component_type_t, typename type_id_t>
		void constructComponentPayload(slot_t& slot, const component_type_t& type, const void* init_data, type_id_t type_id, const char* component_name)
		{
			slot.uses_external_data = !type.inline_relocatable || type.data_size > slot_t::inline_data_capacity;
			if(slot.uses_external_data)
			{
				slot.external_data = (danikk_framework::malloc)(type.data_size);
				if(slot.external_data == nullptr)
					danikk_engine::formatFatalError("Cannot allocate % bytes for % component type %", type.data_size, component_name, static_cast<uint32>(type_id));
			}
			type.construct(slot.getData(), init_data);
		}
	}

	/** @copydoc WorldBodyData::createWorldBody */
	WorldBodyId WorldBodyData::createWorldBody() { return world_bodies.create(); }

	/** @copydoc WorldBodyData::createWorldBodyComponentSlot */
	WorldBodyComponentSlotId WorldBodyData::createWorldBodyComponentSlot(WorldBodyId world_body_id, WorldBodyComponentTypeId type_id, const void* init_data)
	{
		if(!isWorldBodyActive(world_body_id)) danikk_engine::formatFatalError("World body component references invalid body %", world_body_id);
		const WorldBodyComponentType& type = getWorldBodyComponentType(type_id);
		validateComponentType(type, type_id, "World body");
		const WorldBodyComponentSlotId id = world_body_components.create();
		WorldBodyComponentSlot& slot = world_body_components[id];
		slot.slot_id = id;
		slot.world_body_id = world_body_id;
		slot.generation_id = next_world_body_component_generation_id++;
		slot.type_id = type_id;
		constructComponentPayload(slot, type, init_data, type_id, "world body");
		getWorldBody(world_body_id).addComponentSlot(id);
		return id;
	}
}
