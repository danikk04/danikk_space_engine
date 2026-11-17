#pragma once

namespace danikk_space_engine
{
	/**
	 * @brief Runs a headless ground-support scenario with 100 NPCs per faction and logs the measurements.
	 * @return Zero if the NPCs settle, stand, walk and lose support after excavation as expected.
	 */
	int runNpcDiagnostics();
}
