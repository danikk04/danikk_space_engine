#pragma once
#include <object/entity/particle.h>
#include <asset.h>

namespace danikk_space_engine
{
	class FlameParticle : public Particle
	{
		void frame() override
		{
			Object::frame();
			setWorldMatrix(world_matrix);
			setDrawColor(vec4(1.0f, 0.0f, 0.0f, 1.0f));
			white_texture.bind();
			static_asset_collection.low_poly_sphere_mesh.draw();
		}
	};
}
