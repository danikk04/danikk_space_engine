#pragma once

#include <default.h>
#include <object/object.h>
#include <object/world.h>

namespace game
{
	struct CollisionInfo
	{
		vec3 pos;
		vec3 rotation;
		float pulse;
	};

	class Collider : public virtual WorldObject
	{
		float max_radius;

		virtual bool checkCollision(const Collider& other_collider, CollisionInfo& out);

		virtual void processCollision(const CollisionInfo& collision);
	};
}
