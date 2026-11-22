#include <danikk_space_engine/systems/telemetry/telemetry.h>

namespace danikk_space_engine
{
	TelemetrySystem::TelemetrySystem() = default;

	void TelemetrySystem::beginTick()
	{
		profiling_system.beginTick();
		mesh_generation_telemetry.beginFrame();
	}

	void TelemetrySystem::endTick(bool force_report)
	{
		mesh_generation_telemetry.endFrame();
		profiling_system.endTick(force_report);
	}

	void TelemetrySystem::setReportIntervalMilliseconds(uint64 milliseconds)
	{
		profiling_system.setReportIntervalMilliseconds(milliseconds);
	}
}
