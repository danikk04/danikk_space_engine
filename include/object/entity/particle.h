#pragma once
#include <object/entity/entity.h>
#include <object/block_map.h>
#include <object/timed.h>
#include <block/data.h>
#include <manager.h>

namespace danikk_space_engine
{
	class Particle : public virtual PhysicObject, public virtual TimedObject
	{
		virtual void collision(BlockSlot& block){};

		void tick() override
		{
			PhysicObject::tick();
			TimedObject::tick();
			BlockMapObject* parent_as_block_map = dynamic_cast<BlockMapObject*>(getParent());
			assert(parent_as_block_map != NULL);

			pos_type block_pos = pos;
			GlobalRegionScope scope1;
			GlobalChunkScope scope2;
			BlockSlot* block = parent_as_block_map->getBlock(block_pos, scope1, scope2);

			if(block != NULL)
			{
				if(block->isHeaderExits())
				{
					speed = vec3(0.0f);
					collision(*block);
					//exits = false;
				}

			}
			else
			{
				exits = false;
			}
		}
	public:
		Particle()
		{
			tick_to_live = 120;
		}
	};
}
