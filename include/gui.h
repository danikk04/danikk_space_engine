#pragma once

#include <danikk_engine/gui.h>
#include <danikk_engine/fps.h>
#include <danikk_engine/style.h>
#include "default.h"

namespace danikk_space_engine
{
	class GUI
	{
		Label fps_label;
		Label debug_info_label;
	public:
		void define();

		void compose();

		void frame();
	};

	extern GUI gui;
}
