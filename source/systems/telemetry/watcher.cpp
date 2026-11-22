#include <danikk_space_engine/systems/telemetry/profiling.h>
#include <danikk_space_engine/systems/telemetry/watcher.h>

namespace danikk_space_engine
{
	Watcher::Watcher(const char* name)
		: profiling_system(ProfilingSystem::getCurrentThreadSystem()),
		  node_id(profiling_system == nullptr ? static_cast<uint32>(-1)
			: profiling_system->beginScope(name == nullptr ? "(unnamed)" : name)),
		  start_time(std::chrono::steady_clock::now())
	{
	}

	Watcher::~Watcher()
	{
		const auto elapsed = std::chrono::steady_clock::now() - start_time;
		const uint64 elapsed_nanoseconds = static_cast<uint64>(
			std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count());
		if(profiling_system != nullptr) profiling_system->endScope(node_id, elapsed_nanoseconds);
	}
}
