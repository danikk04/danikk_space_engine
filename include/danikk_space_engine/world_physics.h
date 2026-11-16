#pragma once

#include <danikk_space_engine/fwd.h>

namespace danikk_space_engine
{
	class WorldPhysics
	{
		void integrateMotion(World& world);
		void destroyEntitiesInsidePlanetCore(World& world);
		void resolvePlanetCollisions(World& world);
		void resolveBodyCollisions(World& world);
		void resolveHumanCollisions(World& world);

	public:
		void tick(World& world);
	};
}
