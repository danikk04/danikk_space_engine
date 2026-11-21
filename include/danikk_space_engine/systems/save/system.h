#pragma once

#include <danikk_space_engine/fwd.h>
#include <danikk_space_engine/id.h>

namespace danikk_space_engine
{
	/** @brief Owns world-save paths and serializes persistent world data. */
	class SaveSystem
	{
		World& world;

	public:
		/** @brief Binds save operations to their owning world. */
		explicit SaveSystem(World& world) : world(world) {}
		/** @brief Logs initialization of the save subsystem. */
		void init();
		/** @brief Logs shutdown of the save subsystem. */
		void shutdown();

		/** @brief Loads a planet's saved terrain edits into its registered PlanetData. */
		bool loadPlanetTerrainEdits(PlanetId planet_id);
		/** @brief Writes a planet's ordered terrain edits and revision to its save file. */
		void savePlanetTerrainEdits(PlanetId planet_id);
	};
}
