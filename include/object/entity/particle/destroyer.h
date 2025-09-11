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
			assert(current_block_context->block->getId() != 0);
			current_block_context->block->getHeader().id = 0;
			current_block_context->block->data.resize(0);
			assert(current_block_context->block->getId() == 0);
			BlockMapChunk* current_chunk = current_block_context->chunk;
			block_collection_flags* chunk_flags = &current_chunk->flags;
			chunk_flags->is_mesh_changed = true;
			chunk_flags->is_active = true;
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
