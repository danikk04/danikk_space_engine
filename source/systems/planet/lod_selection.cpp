#include <danikk_space_engine/profiler.h>
#include <danikk_space_engine/systems/planet/system.h>

#include <danikk_framework/log.h>

namespace danikk_space_engine
{
	namespace
	{
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
		const uint8 previous_level = planet.target_lod_level;
		uint8 target_level = previous_level;
		if(target_level == 0)
		{
			target_level = 1;
			while(target_level < last_lod_level
				&& height >= getPlanetLodSettings(target_level + 1)
					.getActivationHeight(settings.radius))
				target_level++;
		}
		else
		{
			while(target_level < last_lod_level
				&& height >= getPlanetLodSettings(target_level + 1)
					.getActivationHeight(settings.radius))
				target_level++;
			while(target_level > 1
				&& height < getPlanetLodSettings(target_level)
					.getDeactivationHeight(settings.radius))
				target_level--;
		}
		planet.target_lod_level = target_level;
		if(target_level != previous_level)
			danikk_framework::formatLogInfo(
				"Planet LOD target level changed: body=% level=% height_above_surface=% m",
				planet.world_body_id, target_level, height);
		return target_level;
	}

	uint8 PlanetSystem::selectPlanetLodRenderLevel(const PlanetData& planet, uint8 target_level) const
	{
		DSE_WATCH();
		if(target_level == 0) return 0;
		if(isLevelReady(planet.levels[target_level])) return target_level;

		const uint8 previous_level = planet.lod_level;
		if(previous_level > 0 && previous_level < planet.levels.size()
			&& planet.levels[previous_level].mesh != nullptr)
			return previous_level;
		if(planet.levels[target_level].mesh != nullptr) return target_level;
		const uint8 first_global_level = findFirstGlobalPlanetLodLevel();
		if(first_global_level < planet.levels.size() && target_level != first_global_level
			&& isLevelReady(planet.levels[first_global_level]))
			return first_global_level;
		return 0;
	}
}
