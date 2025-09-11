#pragma once
#include <object/entity/particle.h>
#include <asset.h>
#include <block/data.h>

namespace danikk_space_engine
{
	class PlasmaBullet : public Particle
	{
		void collision() override
		{
			BlockBaseHeader& header = current_block_context->block->getHeader();
			header.temperature += mass * temperature / header.main_material_mass;
		}

		void frame() override
		{
			Object::frame();
			setWorldMatrix(world_matrix);
			setDrawColor(vec4(1.0f, 0.0f, 0.0f, 1.0f));
			white_texture.bind();
			static_asset_collection.low_poly_sphere_mesh.draw();
		}
	public:
		static constexpr const char* name = "plasma";
		static size_t id;

		PlasmaBullet()
		{
			size = vec3(0.1f, 0.03f, 0.03f);
			mass = 0.001f;
			rotation_speed = vec3(0.0f);
			temperature = 1000.0f;
			tick_to_live = 500;
		}
	};
}
