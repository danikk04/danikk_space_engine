#pragma once

#include <object/block_map.h>
#include <object/object.h>
#include <block/context.h>
#include <block/pos.h>

#include <block/data.h>
#include <manager.h>
#include "limited_live.h"

namespace danikk_space_engine
{
	typedef void (*block_collision_event_t)();
	//void (*atEntityCollision)();

	inline BlockContext getBlockAt()
	{
		useParentObjectTag(BlockMap, block_map);
		useCurrentObjectTag(World, world);
		assert(block_map_tag != NULL);

		global_pos_type global_pos;
		global_pos.setGlobalPos(world_tag->pos);
		return block_map_tag->get(global_pos);
	}

	namespace object_tags
	{
		class PointCollider : public ObjectTag
		{
		public:
			static uint32 id;

			block_collision_event_t block_collision_event;

			void tick()
			{
				useCurrentObjectTag(World, world);
				useCurrentObjectTag(Kinetic, physic);

				if(world_tag->pos.x < 0 || world_tag->pos.y < 0 || world_tag->pos.z < 0)
				{
					getCurrentObject()->dispose();
					return;
				}
				if(physic_tag->speed != vec3(0.0f))
				{
					BlockContext block_context = getBlockAt();
					BlockContextUser user(&block_context);

					if(current_block_context->block != NULL)
					{
						if(current_block_context->block->id != 0)
						{
							if(block_collision_event != NULL)
							{
								block_collision_event();
							}
							getCurrentObject()->dispose();
						}
					}
					else
					{
						getCurrentObject()->dispose();
					}
				}
			}
		};
	}
}
