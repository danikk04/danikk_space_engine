#pragma once
#include <object/entity/entity.h>
#include <object/block_map.h>
#include <object/timed.h>
#include <block/context.h>
#include <block/pos.h>

#include <block/data.h>
#include <manager.h>

namespace danikk_space_engine
{
	class Particle : public virtual PhysicObject, public virtual TimedObject
	{
		virtual void collision(){};

		void tick() override
		{
			if(pos.x < 0 || pos.y < 0 || pos.z < 0)
			{
				dispose();
				return;
			}
			PhysicObject::tick();
			TimedObject::tick();
			if(speed != vec3(0.0f))
			{
				BlockContext block_context = getBlockAt();
				BlockContextUser user(&block_context);

				if(current_block_context->block != NULL)
				{
					if(current_block_context->block->isHeaderExits())
					{
						collision();
						speed = vec3(0.0f);
						//dispose();
					}
				}
				else
				{
					dispose();
				}
			}
		}
	public:
		BlockContext getBlockAt()
		{
			BlockMapObject* parent_as_block_map = dynamic_cast<BlockMapObject*>(getParent());
			assert(parent_as_block_map != NULL);

			global_pos_type global_pos;
			global_pos.setGlobalPos(pos);
			return parent_as_block_map->get(global_pos);
		}

		Particle()
		{
			tick_to_live = 120;
		}
	};

	struct particle_type
	{
		const char* particle_name;
		void* (*constructor)();

		particle_type() = default;

		particle_type(const char* particle_name, void* (*constructor)()) : particle_name(particle_name), constructor(constructor){}
	};


}
