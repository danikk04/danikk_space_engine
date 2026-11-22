#include <danikk_space_engine/systems/telemetry/profiling.h>
#include <danikk_space_engine/systems/telemetry/watcher.h>

namespace danikk_space_engine
{
	std::atomic<ProfilingSystem*> Watcher::active_profiling_system = nullptr;

	Watcher::Watcher(const char* name)
		: profiling_system(active_profiling_system.load(std::memory_order_acquire)),
		  node_id(profiling_system == nullptr
			|| std::this_thread::get_id() != profiling_system->main_thread_id
			? static_cast<uint32>(-1)
			: profiling_system->beginScope(name == nullptr ? "(unnamed)" : name)),
		  start_time(std::chrono::steady_clock::now())
	{
		if(node_id == static_cast<uint32>(-1)) profiling_system = nullptr;
	}

	Watcher::~Watcher()
	{
		const auto elapsed = std::chrono::steady_clock::now() - start_time;
		const uint64 elapsed_nanoseconds = static_cast<uint64>(
			std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count());
		if(profiling_system != nullptr) profiling_system->endScope(node_id, elapsed_nanoseconds);
	}
}
