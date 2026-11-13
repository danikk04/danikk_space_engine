#pragma once

#include <danikk_space_engine/point.h>

namespace danikk_space_engine
{
	enum class PointComponentType : uint8
	{
		none = 0,
		camera,
		machine_gun,
		engine
	};

	struct PointComponent
	{
		static constexpr size_t inline_data_capacity = 32;

		PointId point_id = 0;
		PointComponentType type = PointComponentType::none;
		bool uses_external_data = false;
		union
		{
			uint8 inline_data[inline_data_capacity]{};
			void* external_data;
		};
	};
}
