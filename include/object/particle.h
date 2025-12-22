#pragma once

#include <object/block_map.h>
#include <object/timed.h>
#include <object/object.h>
#include <block/context.h>
#include <block/pos.h>

#include <block/data.h>
#include <manager.h>

namespace danikk_space_engine
{
	class Particle : public ObjectTag
	{
	public:
		void (*atBlockCollision)();
		void (*atEntityCollision)();

		void tick() override
		{
			useObjectTag(World, world);
			useObjectTag(Kinetic, physic);

			if(world_tag->pos.x < 0 || world_tag->pos.y < 0 || world_tag->pos.z < 0)
			{
				dispose();
				return;
			}
			if(physic_tag->speed != vec3(0.0f))
			{
				BlockContext block_context = getBlockAt();
				BlockContextUser user(&block_context);

				if(current_block_context->block != NULL)
				{
					if(current_block_context->block->isHeaderExits())
					{
						collision();
						speed = vec3(0.0f);
						dispose();
					}
				}
				else
				{
					dispose();
				}
			}
		}
		BlockContext getBlockAt()
		{
			BlockMapObject* parent_as_block_map = dynamic_cast<BlockMapObject*>(getParent());
			assert(parent_as_block_map != NULL);

			global_pos_type global_pos;
			global_pos.setGlobalPos(pos);
			return parent_as_block_map->get(global_pos);
		}

		void frame() override
		{
			Object::frame();
			setWorldMatrix(world_matrix);
			setDrawColor(vec4(0.0f, 0.0f, 0.0f, 1.0f));
			white_texture.bind();
			static_asset_collection.low_poly_sphere_mesh.draw();
		}

		Particle()
		{
			tick_to_live = 120;
		}
	};
}
