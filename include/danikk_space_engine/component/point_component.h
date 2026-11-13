#pragma once

#include <cstddef>

#include <danikk_space_engine/point.h>

namespace danikk_space_engine
{
	class World;
	struct PointComponentSlot;
	using PointComponentSlotId = uint32;

	enum class PointComponentTypeId : uint32
	{
		none = 0,
		camera,
		machine_gun,
		engine,
		count
	};

	using PointComponentConstructFunction = void (*)(void* destination, const void* init_data);
	using PointComponentMoveFunction = void (*)(void* destination, void* source);
	using PointComponentDestroyFunction = void (*)(void* data);
using PointComponentUpdateFunction = void (*)(PointComponentSlot& slot, World& world);

	struct PointComponentType
	{
		size_t data_size = 0;
		size_t data_alignment = 1;
		PointComponentConstructFunction construct = nullptr;
		PointComponentMoveFunction move = nullptr;
		PointComponentDestroyFunction destroy = nullptr;
		PointComponentUpdateFunction tick = nullptr;
		PointComponentUpdateFunction frame = nullptr;
	};

	const PointComponentType& getPointComponentType(PointComponentTypeId type_id);

	struct PointComponentSlot
	{
		static constexpr size_t inline_data_capacity = 32;

		PointId point_id = 0;
		PointComponentTypeId type_id = PointComponentTypeId::none;
		bool uses_external_data = false;
		union
		{
			alignas(std::max_align_t) uint8 inline_data[inline_data_capacity]{};
			void* external_data;
		};

		void* getData()
		{
			return uses_external_data ? external_data : inline_data;
		}

		const void* getData() const
		{
			return uses_external_data ? external_data : inline_data;
		}
	};
}
