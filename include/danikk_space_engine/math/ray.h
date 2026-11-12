#pragma once

#include <danikk_space_engine/default.h>

namespace danikk_space_engine
{
	struct Ray
	{
		vec3 origin;
		vec3 direction;
	};

	struct RayTriangleIntersection
	{
		bool hit = false;
		float distance = 0.0f;
		vec3 normal = vec3(0.0f);
	};

	RayTriangleIntersection intersectRayTriangle(
		const Ray& ray,
		const vec3& a,
		const vec3& b,
		const vec3& c);
}
