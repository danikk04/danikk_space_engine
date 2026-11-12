#pragma once

#include <danikk_space_engine/default.h>

#include <danikk_space_engine/point.h>
#include <danikk_space_engine/controller.h>
#include <danikk_framework/dynamic_array.h>

namespace danikk_space_engine
{
	class World
	{
	public:
		DynamicArray<Point> points;
		LocalPlayerController controller;

		void tick();

		void frame();

		void init();
	};

	extern World world;
}
