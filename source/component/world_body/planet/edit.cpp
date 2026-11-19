#include <danikk_space_engine/planet_system.h>
#include <danikk_space_engine/world/world.h>

namespace danikk_space_engine
{

	/** @copydoc editPlanetGround */
	bool editPlanetGround(World& world, WorldBodyId body_id, const glm::dvec3& position,
		double radius, bool adds_solid_terrain)
	{
		return world.planets.editGround(body_id, position, radius, adds_solid_terrain);
	}
}
