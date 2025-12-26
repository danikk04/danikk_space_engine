#pragma once

#include <default.h>
#include <object/object.h>
#include <object/world.h>
#include <object/kinetic.h>
#include <object/meshed.h>
#include <object/block_map.h>
#include <object/point_collider.h>
#include <danikk_engine/mesh.h>
#include <block/material.h>
#include <asset.h>
#include "limited_live.h"

namespace danikk_space_engine
{
	typedef void (*block_collision_event_t)();

	inline void destroyBlockCollision()
	{
		current_block_context->map->destroyBlock();
	}

	inline void plasmaBlockCollision()
	{
		//useCurrentObjectTag(World, world);
		useCurrentObjectTag(Kinetic, kinetic);
		useObjectTag(current_block_context->block, BlockMaterial, material);
		material_tag->temperature += kinetic_tag->mass * kinetic_tag->temperature / material_tag->main_material_mass;
	}

	inline void fillerBlockCollision()
	{
		useCurrentObjectTag(World, world);
		useCurrentObjectTag(Kinetic, kinetic);

		BlockContext current_context = getBlockAt();

		if(current_context.block->id != 0)
		{
			world_tag->pos -= kinetic_tag->speed * getTargetFrameDelay() * 2.0f;//откатываем позицию снаряда на 2 тика

			//BlockContext prev_context = getBlockAt();
			//current_context.block->data.copyTo(prev_context.block->data, current_context.region->getAllocator());
			logWarning("UNIMP OF COPY fillerBlockCollision");
			current_context.chunk->flags.setChanged();
		}
		getCurrentObject()->dispose();
	}

	inline void selecterBlockCollision()
	{

	}

	template<class collider_t> Object* createProjectile(vec3 pos, vec3 rotation, vec3 size, vec4 color, float speed, block_collision_event_t block_collision_event)
	{
		using namespace object_tags;
		Object* projectile = Object::create<World, Kinetic, Meshed, LimitedLive, collider_t>();
		World* 		world_tag = 	projectile->getTag<World>(0);
		Kinetic* 	kinetic_tag = 	projectile->getTag<Kinetic>(1);
		Meshed* 	meshed_tag = 	projectile->getTag<Meshed>(2);
		LimitedLive* 		timed_tag = 	projectile->getTag<LimitedLive>(3);
		collider_t*	collider_tag = 	projectile->getTag<collider_t>(4);

		world_tag->size = size;
		world_tag->pos = pos;
		world_tag->rotation = rotation;

		//kinetic_tag->rotation_speed = vec3(0.0f);

		kinetic_tag->speed = getFront(world_tag->rotation) * speed;
		kinetic_tag->mass = 0.1f;
		kinetic_tag->temperature = 100.0f;

		meshed_tag->texture = white_texture;
		meshed_tag->color = color;
		meshed_tag->mesh = &static_asset_collection.low_poly_sphere_mesh;

		timed_tag->tick_to_live = 500;

		collider_tag->block_collision_event = block_collision_event;
		return projectile;
	}

	inline Object* createDestroyerProjectile(vec3 pos, vec3 rotation)
	{
		return createProjectile<object_tags::PointCollider>(pos, rotation, vec3(0.1f, 0.03f, 0.03f), vec4(0.0f, 0.0f, 0.0f, 1.0f), 2.0f, destroyBlockCollision);
	}

	inline Object* createPlasmaProjectile(vec3 pos, vec3 rotation)
	{
		return createProjectile<object_tags::PointCollider>(pos, rotation, vec3(0.1f, 0.03f, 0.03f), vec4(1.0f, 0.0f, 0.0f, 1.0f), 1.0f, plasmaBlockCollision);
	}
}
