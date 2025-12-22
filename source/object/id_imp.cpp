#include <controller/player.h>
#include <object/block_map.h>
#include <object/tag_table.h>
#include "../../include/object/kinetic.h"

namespace danikk_space_engine
{
	uint32 object_tags::Kinetic::id;
	uint32 object_tags::World::id;
	uint32 object_tags::BlockMap::id;
	uint32 object_tags::PlayerController::id;

	using namespace internal;

	void generateTagsId()
	{
		object_tags::Kinetic::id = generateTagId(); addTag<object_tags::Kinetic>();
		object_tags::World::id = generateTagId(); addTag<object_tags::World>();
		object_tags::BlockMap::id = generateTagId(); addTag<object_tags::BlockMap>();
		object_tags::PlayerController::id = generateTagId(); addTag<object_tags::PlayerController>();
	}
}
