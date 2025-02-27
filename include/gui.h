#pragma once

#include <danikk_engine/gui.h>
#include <danikk_engine/fps.h>
#include <danikk_engine/style.h>
#include "default.h"

namespace sotiria_project
{
	class GUI
	{
		Label fps_label;
	public:
		void define();

		void compose();

		void frame();
	};

	extern GUI gui;
}
