#pragma once
#include <object/entity/entity.h>
#include <object/block_map.h>
#include <block/data.h>
#include <manager.h>

namespace danikk_space_engine
{
	class Particle : public PhysicObject
	{
		virtual void collision(BlockSlot& block){};

		void tick() override
		{
			PhysicObject::tick();
			BlockMapObject* parent_as_block_map = dynamic_cast<BlockMapObject*>(getParent());
			assert(parent_as_block_map != NULL);

			pos_type block_pos = pos;
			BlockSlot* block = parent_as_block_map->getBlock(block_pos);
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
			current_region = NULL;
		}
	};
}
