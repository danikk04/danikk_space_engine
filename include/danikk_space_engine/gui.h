#pragma once

#include <danikk_engine/gui.h>
#include <danikk_engine/fps.h>
#include <danikk_engine/style.h>
#include <danikk_space_engine/default.h>

namespace danikk_space_engine
{

	/**
	 * @brief Represents gui data and behavior.
	 */
	class GUI
	{
		GUIElement debug_info_label;
		size_t memory_sample_bytes = 0;
		float memory_sample_timer = 1.0f;
	public:

		/**
		 * @brief Performs the define operation.
		 */
		void define();

		/**
		 * @brief Performs the compose operation.
		 */
		void compose();

		/**
		 * @brief Performs the frame operation.
		 */
		void frame();
	};

	extern GUI gui;
}
