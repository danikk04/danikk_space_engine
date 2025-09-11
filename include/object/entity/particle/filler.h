#pragma once
#include <object/entity/particle.h>
#include <asset.h>
#include <block/data.h>
#include <block/pos.h>

namespace danikk_space_engine
{
	class FillerBullet : public Particle
	{
		void collision() override
		{
			BlockContext current_context = getBlockAt();

			if(current_context.block->getId() != 0)
			{
				pos -= speed * getTargetFrameDelay() * 2.0f;//откатываем позицию снаряда на 2 тика

				BlockContext prev_context = getBlockAt();

				current_context.block->data.copyTo(prev_context.block->data, current_context.region->getAllocator());
				current_context.chunk->flags.is_mesh_changed = true;
				current_context.chunk->flags.is_active = true;
			}
			dispose();
		}

		void frame() override
		{
			Object::frame();
			setWorldMatrix(world_matrix);
			setDrawColor(vec4(0.0f, 0.0f, 0.0f, 1.0f));
			white_texture.bind();
			static_asset_collection.low_poly_sphere_mesh.draw();
		}
	public:
		static constexpr const char* name = "filler";
		static size_t id;

		FillerBullet()
		{
			size = vec3(0.1f, 0.03f, 0.03f);
			mass = 0.001f;
			rotation_speed = vec3(0.0f);
			temperature = 1000.0f;
		}
	};
}
