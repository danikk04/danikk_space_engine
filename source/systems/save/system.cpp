#include <danikk_framework/log.h>
#include <danikk_space_engine/systems/save/system.h>

namespace danikk_space_engine
{
	void SaveSystem::init()
	{
		danikk_framework::formatLogInfo("Init SaveSystem");
	}

	void SaveSystem::shutdown()
	{
		danikk_framework::formatLogInfo("Shutdown SaveSystem");
	}
}
