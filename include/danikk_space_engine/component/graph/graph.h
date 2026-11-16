#pragma once

#include <cstddef>

#include <danikk_space_engine/fwd.h>
#include <danikk_space_engine/id.h>
#include <danikk_space_engine/world/point.h>

namespace danikk_space_engine
{
	enum class GraphComponentTypeId : uint32
	{
		none = 0,
		pipe,
		wire,
		count
	};

	using GraphComponentConstructFunction = void (*)(void* destination, const void* init_data);
	using GraphComponentMoveFunction = void (*)(void* destination, void* source);
	using GraphComponentDestroyFunction = void (*)(void* data);
	using GraphComponentUpdateFunction = void (*)(GraphComponentSlot& slot, World& world);

	/**
	 * @brief Defines graph-component storage size and lifecycle, tick, and frame callbacks.
	 */
	struct GraphComponentType
	{
		size_t data_size = 0;
		size_t data_alignment = 1;
		bool inline_relocatable = false;
		GraphComponentConstructFunction construct = nullptr;
		GraphComponentMoveFunction move = nullptr;
		GraphComponentDestroyFunction destroy = nullptr;
		GraphComponentUpdateFunction tick = nullptr;
		GraphComponentUpdateFunction frame = nullptr;
	};

	/**
	 * @brief Returns graph component type.
	 */
	const GraphComponentType& getGraphComponentType(GraphComponentTypeId type_id);

	/**
	 * @brief Associates component data and callbacks with a pair of world points.
	 */
	struct GraphComponentSlot
	{
		static constexpr size_t inline_data_capacity = 32;

		PointId point_a = 0;
		PointId point_b = 0;
		GraphComponentTypeId type_id = GraphComponentTypeId::none;
		bool uses_external_data = false;
		union
		{
			alignas(std::max_align_t) uint8 inline_data[inline_data_capacity]{};
			void* external_data;
		};

		/**
		 * @brief Returns the component payload, selecting inline storage or the external allocation recorded by the slot.
		 */
		void* getData()
		{
			return uses_external_data ? external_data : inline_data;
		}

		/**
		 * @brief Returns the component payload, selecting inline storage or the external allocation recorded by the slot.
		 */
		const void* getData() const
		{
			return uses_external_data ? external_data : inline_data;
		}

		/**
		 * @brief Returns the component slot to its default unused state.
		 */
		void dispose()
		{
			*this = GraphComponentSlot();
		}
	};
}
