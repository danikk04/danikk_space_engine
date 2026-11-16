#pragma once

#include <danikk_engine/gui.h>
#include <danikk_engine/fps.h>
#include <danikk_engine/style.h>
#include <danikk_space_engine/default.h>

namespace danikk_space_engine
{

	/**
	 * @brief Owns the top-left debug label and periodically formats live runtime values.
	 *
	 * The GUI text reports FPS, process memory, active tool/action, camera speed,
	 * and camera position. Process memory is sampled at roughly one-second
	 * intervals; a zero reading is displayed as unavailable.
	 */
	class GUI
	{
		/** @brief Engine GUI element used to display the game's multiline debug readout. */
		GUIElement debug_info_label;
		/** @brief Most recently sampled process memory usage in bytes; zero means unavailable. */
		size_t memory_sample_bytes = 0;
		/** @brief Elapsed frame time since the previous process-memory sample. */
		float memory_sample_timer = 1.0f;
	public:

		/**
		 * @brief Configures the debug label’s font, multiline layout, character size, and position.
		 *
		 * Uses the shared bitmap font and 0.014 screen-relative character dimensions. The label is
		 * anchored at the top-left and given a 0.4 by 0.3 relative layout area. The style object is
		 * shared outside this class and must remain alive while the label references it.
		 */
		void define();

		/**
		 * @brief Adds the configured debug label to the engine GUI root.
		 * Call after define() so the root receives the configured text style and anchor.
		 */
		void compose();

		/**
		 * @brief Refreshes sampled memory and debug text for the current application frame.
		 *
		 * Samples process memory when the accumulated target-frame delay reaches one second, then
		 * builds one multiline string in this order: FPS, RAM in MiB, active tool, selected action,
		 * camera speed in m/s, and camera position truncated to hundredths. A zero memory sample is
		 * shown as unavailable. Rendering of the label is driven by the surrounding engine frame loop.
		 */
		void frame();
	};

	extern GUI gui;
}
