#include <danikk_space_engine/component/world_body/world_body.h>

namespace danikk_space_engine
{
	namespace
	{
		/**
		 * @brief Static callback registry indexed by WorldBodyComponentTypeId.
		 *
			 * The `none` entry is intentionally empty. Planet state now belongs to
			 * PlanetSystem and is not a world-body component.
		 */
		const WorldBodyComponentType world_body_component_types[] =
		{
			{} // none
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
