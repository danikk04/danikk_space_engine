#include <danikk_space_engine/profiler.h>
#include <danikk_space_engine/systems/planet/system.h>

#include <danikk_framework/log.h>

namespace danikk_space_engine
{
	namespace
	{
		constexpr uint8 first_global_heightmap_level = 4;
		constexpr uint8 last_lod_level = planet_chunk_level_count - 1;

		bool isLevelReady(const PlanetChunkLevel& level)
		{
			if(!level.initialized || level.mesh == nullptr || level.expected_patch_count == 0
				|| level.patches.size() != level.expected_patch_count) return false;
			for(const PlanetLodPatch& patch : level.patches)
				if(!patch.ready) return false;
			return true;
		}
	}

	uint8 PlanetSystem::selectPlanetLodTargetLevel(PlanetData& planet, double height,
		const PlanetSettings& settings)
	{
		DSE_WATCH();
		const double level_widths[planet_chunk_level_count] = {
			0.0, planet_chunk_width * 8.0,
			planet_chunk_width * 64.0, planet_chunk_width * 1024.0,
			0.0, 0.0};
		const double thresholds[planet_chunk_level_count - 2] = {
			glm::max(settings.chunk_load_radius * 4.0, level_widths[1] * 2.5),
			glm::max(settings.radius * 0.003, level_widths[2] * 2.0),
			glm::max(settings.radius * 0.05, level_widths[3] * 2.0),
			settings.radius * 0.75};

		const uint8 previous_level = planet.lod_level;
		uint8 target_level = previous_level;
		if(target_level == 0)
		{
			target_level = 1;
			while(target_level < last_lod_level
				&& height >= thresholds[target_level - 1])
				target_level++;
		}
		else
		{
			while(target_level < last_lod_level
				&& height >= thresholds[target_level - 1] * 1.1)
				target_level++;
			while(target_level > 1
				&& height < thresholds[target_level - 2] * 0.9)
				target_level--;
		}
		if(target_level != previous_level)
			danikk_framework::formatLogInfo(
				"Planet LOD target level changed: body=% level=% height_above_surface=% m",
				planet.world_body_id, target_level, height);
		return target_level;
	}

	uint8 PlanetSystem::selectPlanetLodRenderLevel(const PlanetData& planet, uint8 target_level) const
	{
		DSE_WATCH();
		if(isLevelReady(planet.levels[target_level])) return target_level;

		const uint8 previous_level = planet.lod_level;
		if(previous_level > 0 && previous_level < planet.levels.size()
			&& planet.levels[previous_level].mesh != nullptr)
			return previous_level;
		if(planet.levels[target_level].mesh != nullptr) return target_level;
		if(target_level != first_global_heightmap_level
			&& isLevelReady(planet.levels[first_global_heightmap_level]))
			return first_global_heightmap_level;
		return 0;
	}
}
