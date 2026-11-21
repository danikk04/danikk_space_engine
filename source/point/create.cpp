#include <danikk_space_engine/point.h>

namespace danikk_space_engine
{
	namespace
	{
		/** @brief Rejects invalid point-component registrations before allocation. */
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

	/** @copydoc PointData::createPoint */
	PointId PointData::createPoint() { return points.create(); }

	/** @copydoc PointData::createPointComponentSlot */
	PointComponentSlotId PointData::createPointComponentSlot(PointId point_id, PointComponentTypeId type_id, const void* init_data)
	{
		if(!isPointActive(point_id)) danikk_engine::formatFatalError("Point component references invalid point %", point_id);
		const PointComponentType& type = getPointComponentType(type_id);
		validateComponentType(type, type_id, "Point");
		const PointComponentSlotId id = point_components.create();
		PointComponentSlot& slot = point_components[id];
		slot.point_id = point_id;
		slot.chunk_id = PointComponentSlot::no_chunk;
		slot.type_id = type_id;
		slot.generation_id = next_point_component_generation_id++;
		if(next_point_component_generation_id == 0) next_point_component_generation_id = 1;
		constructComponentPayload(slot, type, init_data, type_id, "point");
		points[point_id].component_slot_ids.push(id);
		return id;
	}
}
