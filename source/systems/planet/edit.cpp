#include <algorithm>
#include <cmath>
#include <istream>
#include <ostream>

#include <danikk_space_engine/systems/planet/system.h>
#include <danikk_space_engine/world.h>

namespace danikk_space_engine
{
	/** @copydoc PlanetTerrainEdit::PlanetTerrainEdit */
	PlanetTerrainEdit::PlanetTerrainEdit(
		const glm::dvec3& center, double radius, bool adds_solid_terrain)
		: center(center), radius(radius), adds_solid_terrain(adds_solid_terrain)
	{
	}

	/** @copydoc PlanetTerrainEdit::applyToScalarField */
	double PlanetTerrainEdit::applyToScalarField(const glm::dvec3& position, double density) const
	{
		const double distance = glm::length(position - center);
		return adds_solid_terrain
			? std::max(density, radius - distance)
			: std::min(density, distance - radius);
	}

	/** @copydoc PlanetTerrainEdit::applyToSurfaceRadius */
	double PlanetTerrainEdit::applyToSurfaceRadius(const glm::dvec3& direction, double surface_radius) const
	{
		const double along_ray = glm::dot(center, direction);
		const double perpendicular_squared = glm::max(
			0.0, glm::dot(center, center) - along_ray * along_ray);
		if(perpendicular_squared >= radius * radius) return surface_radius;
		const double half_chord = std::sqrt(radius * radius - perpendicular_squared);
		const double outer_radius = along_ray + half_chord;
		const double inner_radius = glm::max(0.0, along_ray - half_chord);
		if(adds_solid_terrain) return glm::max(surface_radius, outer_radius);
		if(surface_radius >= inner_radius && surface_radius <= outer_radius) return inner_radius;
		return surface_radius;
	}

	/** @copydoc PlanetTerrainEdit::getMaximumRadiusFromOrigin */
	double PlanetTerrainEdit::getMaximumRadiusFromOrigin(double margin) const
	{
		return adds_solid_terrain ? glm::length(center) + radius + margin : 0.0;
	}

	/** @copydoc PlanetTerrainEdit::intersectsAabb */
	bool PlanetTerrainEdit::intersectsAabb(
		const glm::dvec3& minimum, const glm::dvec3& maximum, double expansion) const
	{
		const glm::dvec3 closest = glm::clamp(center, minimum, maximum);
		const double expanded_radius = radius + expansion;
		return glm::dot(center - closest, center - closest) <= expanded_radius * expanded_radius;
	}

	/** @copydoc PlanetTerrainEdit::read */
	bool PlanetTerrainEdit::read(std::istream& input)
	{
		glm::dvec3 loaded_center;
		double loaded_radius = 0.0;
		uint8 loaded_adds_solid_terrain = 0;
		input.read(reinterpret_cast<char*>(&loaded_center.x), sizeof(double));
		input.read(reinterpret_cast<char*>(&loaded_center.y), sizeof(double));
		input.read(reinterpret_cast<char*>(&loaded_center.z), sizeof(double));
		input.read(reinterpret_cast<char*>(&loaded_radius), sizeof(double));
		input.read(reinterpret_cast<char*>(&loaded_adds_solid_terrain), sizeof(loaded_adds_solid_terrain));
		if(!input || !std::isfinite(loaded_radius) || loaded_radius <= 0.0 || loaded_adds_solid_terrain > 1
			|| !std::isfinite(loaded_center.x) || !std::isfinite(loaded_center.y)
			|| !std::isfinite(loaded_center.z)) return false;
		center = loaded_center;
		radius = loaded_radius;
		adds_solid_terrain = loaded_adds_solid_terrain != 0;
		return true;
	}

	/** @copydoc PlanetTerrainEdit::write */
	void PlanetTerrainEdit::write(std::ostream& output) const
	{
		const uint8 serialized_adds_solid_terrain = adds_solid_terrain ? 1 : 0;
		output.write(reinterpret_cast<const char*>(&center.x), sizeof(double));
		output.write(reinterpret_cast<const char*>(&center.y), sizeof(double));
		output.write(reinterpret_cast<const char*>(&center.z), sizeof(double));
		output.write(reinterpret_cast<const char*>(&radius), sizeof(double));
		output.write(reinterpret_cast<const char*>(&serialized_adds_solid_terrain),
			sizeof(serialized_adds_solid_terrain));
	}

	/** @copydoc editPlanetGround */
	bool editPlanetGround(World& world, WorldBodyId body_id, const glm::dvec3& position,
		double radius, bool adds_solid_terrain)
	{
		return world.planets.editGround(body_id, position, radius, adds_solid_terrain);
	}
}
