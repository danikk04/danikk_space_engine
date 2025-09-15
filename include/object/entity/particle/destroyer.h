#pragma once
#include <object/entity/particle.h>
#include <asset.h>
#include <block/data.h>

namespace danikk_space_engine
{
	class DestroyerBullet : public Particle
	{
		void collision() override
		{
			current_block_context->map->destroyBlock();
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
		static constexpr const char* name = "destroyer";
		static size_t id;

		DestroyerBullet()
		{
			size = vec3(0.1f, 0.03f, 0.03f);
			mass = 0.001f;
			rotation_speed = vec3(0.0f);
			temperature = 1000.0f;
		}
	};
}
