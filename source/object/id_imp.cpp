#include <object/player.h>
#include <object/block_map.h>
#include <object/tag_table.h>
#include <object/kinetic.h>
#include <object/world.h>
#include <object/meshed.h>
#include <object/point_collider.h>
#include <object/limited_live.h>

#include <block/material.h>
#include <block/structure_gen.h>

namespace danikk_space_engine
{
	uint32 object_tags::Kinetic::id;
	uint32 object_tags::World::id;
	uint32 object_tags::BlockMap::id;
	uint32 object_tags::PlayerController::id;
	uint32 object_tags::Meshed::id;
	uint32 object_tags::PointCollider::id;
	uint32 object_tags::LimitedLive::id;

	uint32 object_tags::BlockMaterial::id;
	uint32 object_tags::StructureGenBlock::id;

	using namespace internal;


	void generateTagsId()
	{
#define _(name) object_tags::name::id = generateTagId(); addTag<object_tags::name>();
		_(Kinetic)_(World)_(BlockMap)
		_(PlayerController)_(Meshed)_(PointCollider)
		_(LimitedLive)
#undef _
	}
}
