#pragma once
#include <object/entity/particle.h>
#include <asset.h>
#include <block/data.h>

namespace danikk_space_engine
{
	class DestroyerBullet : public Particle
	{
		void collision(BlockSlot& block) override
		{
			block.getHeader().id = 0;
			block.data.resize(0);
			current_chunk->flags.is_mesh_changed = true;
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
		DestroyerBullet()
		{
			size = vec3(0.1f, 0.03f, 0.03f);
			mass = 0.001f;
			rotation_speed = vec3(0.0f);
			temperature = 1000.0f;
		}
	};
}
