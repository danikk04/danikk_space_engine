#include <danikk_space_engine/systems/world_body/system.h>

namespace danikk_space_engine
{
	/** @copydoc WorldBodySystem::createWorldBody */
	WorldBodyId WorldBodySystem::createWorldBody() { return world_bodies.create(); }
}
