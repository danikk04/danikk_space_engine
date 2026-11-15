#pragma once

#include <danikk_framework/dynamic_array.h>
#include <danikk_space_engine/component/point/point.h>
#include <danikk_space_engine/id.h>

namespace danikk_space_engine
{
	struct InventorySlot
	{
		static constexpr ItemDefinitionId empty_item_definition_id = 0;

		ItemDefinitionId item_definition_id = empty_item_definition_id;
		uint32 count = 0;
		uint64 instance_state = 0;
		bool is_equipped = false;

		bool isEmpty() const { return item_definition_id == empty_item_definition_id; }
		void clear() { *this = InventorySlot(); }
	};

	struct InventoryComponent
	{
		DynamicArray<InventorySlot> slots;

		size_t slotCount() const { return slots.size(); }
		void resizeSlots(size_t count) { slots.resize(count); }
	};

	void constructInventoryComponent(void* destination, const void* init_data);
	void moveInventoryComponent(void* destination, void* source);
	void destroyInventoryComponent(void* data);
}
