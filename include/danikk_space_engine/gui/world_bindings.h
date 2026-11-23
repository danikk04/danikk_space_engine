#pragma once

namespace danikk_space_engine
{
	class GUI;
	class World;

	/** @brief Connects a GUI instance to live values owned by a World and its systems. */
	void bindWorldGUI(GUI& gui, World& world);
}
