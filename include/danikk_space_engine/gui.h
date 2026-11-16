#pragma once

#include <danikk_engine/gui.h>
#include <danikk_engine/fps.h>
#include <danikk_engine/style.h>
#include <danikk_space_engine/default.h>

namespace danikk_space_engine
{

	/**
	 * @brief Owns the debug text label and updates its displayed runtime information.
	 */
	class GUI
	{
		GUIElement debug_info_label;
		size_t memory_sample_bytes = 0;
		float memory_sample_timer = 1.0f;
	public:

		/**
		 * @brief Configures the debug label’s font, multiline layout, character size, and position.
		 */
		void define();

		/**
		 * @brief Adds the debug information label to the GUI root.
		 */
		void compose();

		/**
		 * @brief Runs one application frame, including profiling, GUI update, and world rendering.
		 */
		void frame();
	};

	extern GUI gui;
}
