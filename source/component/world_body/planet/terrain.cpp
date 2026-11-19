#include <algorithm>

#include <danikk_space_engine/planet_terrain_system.h>
#include <danikk_space_engine/component/world_body/planet/terrain.h>
#include <danikk_space_engine/world/world.h>

namespace danikk_space_engine
{
	/** @copydoc raycastPlanetScalarField */
	bool raycastPlanetScalarField(const World& world, WorldBodyComponentSlotId planet_component_id,
		const glm::dvec3& origin, const glm::dvec3& direction, double max_distance,
		double& distance, glm::dvec3& normal)
	{
		const double direction_length = glm::length(direction);
		if(direction_length <= 1e-12 || max_distance < 0.0) return false;
		const glm::dvec3 ray_direction = direction / direction_length;
		const auto densityAt = [&](double offset)
		{
			return world.planet_terrain_system.getPlanetDensity(planet_component_id, origin + ray_direction * offset);
		};

		constexpr double step = 0.5;
		double previous_distance = 0.0;
		double previous_density = densityAt(0.0);
		while(previous_distance < max_distance)
		{
			const double next_distance = std::min(previous_distance + step, max_distance);
			const double next_density = densityAt(next_distance);
			if((previous_density <= 0.0 && next_density > 0.0)
				|| (previous_density > 0.0 && next_density <= 0.0))
			{
				double lower = previous_distance;
				double upper = next_distance;
				for(uint8 iteration = 0; iteration < 16; ++iteration)
				{
					const double midpoint = (lower + upper) * 0.5;
					const double midpoint_density = densityAt(midpoint);
					if((midpoint_density > 0.0) == (previous_density > 0.0)) lower = midpoint;
					else upper = midpoint;
				}
				const double hit_distance = (lower + upper) * 0.5;
				const glm::dvec3 hit_position = origin + ray_direction * hit_distance;
				constexpr double gradient_step = 0.05;
				glm::dvec3 gradient(0.0);
				for(uint8 axis = 0; axis < 3; ++axis)
				{
					glm::dvec3 offset(0.0);
					offset[axis] = gradient_step;
					gradient[axis] = world.planet_terrain_system.getPlanetDensity(planet_component_id, hit_position + offset)
						- world.planet_terrain_system.getPlanetDensity(planet_component_id, hit_position - offset);
				}
				const double gradient_length = glm::length(gradient);
				if(gradient_length <= 1e-12) return false;
				distance = hit_distance;
				normal = -gradient / gradient_length;
				return true;
			}
			previous_distance = next_distance;
			previous_density = next_density;
		}
		return false;
	}
}
