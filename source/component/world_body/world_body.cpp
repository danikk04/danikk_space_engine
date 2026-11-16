#include <danikk_space_engine/component/world_body/world_body.h>
#include <danikk_space_engine/component/world_body/planet/planet.h>

namespace danikk_space_engine
{
	namespace
	{
		/**
		 * @brief Static callback registry indexed by WorldBodyComponentTypeId.
		 *
		 * The `none` entry is intentionally empty. The planet entry owns the
		 * PlanetComponent payload lifecycle, updates during frame dispatch, and
		 * accepts completed asynchronous chunk-generation results; it currently
		 * has no per-tick callback.
		 */
		const WorldBodyComponentType world_body_component_types[] =
		{
			{}, // none
			{
				sizeof(PlanetComponent),
				alignof(PlanetComponent),
				false,
				constructPlanetComponent,
				movePlanetComponent,
				destroyPlanetComponent,
				nullptr,
				updatePlanetComponent,
				acceptPlanetGeneratedChunk
			}
		};
	}

	/** @copydoc getWorldBodyComponentType(WorldBodyComponentTypeId) */
	const WorldBodyComponentType& getWorldBodyComponentType(WorldBodyComponentTypeId type_id)
	{
		const uint32 index = static_cast<uint32>(type_id);
		if(index >= static_cast<uint32>(WorldBodyComponentTypeId::count))
		{
			return world_body_component_types[static_cast<uint32>(WorldBodyComponentTypeId::none)];
		}
		return world_body_component_types[index];
	}
}
