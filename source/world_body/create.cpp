#include <danikk_space_engine/world_body.h>

namespace danikk_space_engine
{
	/** @copydoc WorldBodyData::createWorldBody */
	WorldBodyId WorldBodyData::createWorldBody() { return world_bodies.create(); }
}
