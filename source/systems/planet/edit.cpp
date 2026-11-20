#include <algorithm>
#include <cmath>
#include <istream>
#include <ostream>

#include <danikk_space_engine/material.h>
#include <danikk_space_engine/systems/planet/system.h>

namespace danikk_space_engine
{
	/** @copydoc PlanetTerrainEdit::PlanetTerrainEdit */
	PlanetTerrainEdit::PlanetTerrainEdit(
		const glm::dvec3& center, double radius, MaterialId material_id)
		: center(center), radius(radius), material_id(material_id)
	{
	}

	/** @copydoc PlanetTerrainEdit::applyToScalarField */
	double PlanetTerrainEdit::applyToScalarField(const glm::dvec3& position, double density) const
	{
		const double distance = glm::length(position - center);
		return material_id == default_materials::void_material
			? std::min(density, distance - radius)
			: std::max(density, radius - distance);
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
		if(material_id != default_materials::void_material) return glm::max(surface_radius, outer_radius);
		if(surface_radius >= inner_radius && surface_radius <= outer_radius) return inner_radius;
		return surface_radius;
	}

	/** @copydoc PlanetTerrainEdit::applyToMaterial */
	MaterialId PlanetTerrainEdit::applyToMaterial(
		const glm::dvec3& position, MaterialId current_material) const
	{
		if(material_id == default_materials::void_material) return current_material;
		return glm::length(position - center) <= radius ? material_id : current_material;
	}

	/** @copydoc PlanetTerrainEdit::getMaximumRadiusFromOrigin */
	double PlanetTerrainEdit::getMaximumRadiusFromOrigin(double margin) const
	{
		return material_id != default_materials::void_material
			? glm::length(center) + radius + margin : 0.0;
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
	bool PlanetTerrainEdit::read(std::istream& input, uint32 format_version)
	{
		glm::dvec3 loaded_center;
		double loaded_radius = 0.0;
		MaterialId loaded_material_id = default_materials::void_material;
		input.read(reinterpret_cast<char*>(&loaded_center.x), sizeof(double));
		input.read(reinterpret_cast<char*>(&loaded_center.y), sizeof(double));
		input.read(reinterpret_cast<char*>(&loaded_center.z), sizeof(double));
		input.read(reinterpret_cast<char*>(&loaded_radius), sizeof(double));
		if(format_version == 1)
		{
			uint8 loaded_adds_solid_terrain = 0;
			input.read(reinterpret_cast<char*>(&loaded_adds_solid_terrain), sizeof(loaded_adds_solid_terrain));
			if(loaded_adds_solid_terrain > 1) return false;
			loaded_material_id = loaded_adds_solid_terrain
				? default_materials::stone : default_materials::void_material;
		}
		else if(format_version == 2)
			input.read(reinterpret_cast<char*>(&loaded_material_id), sizeof(loaded_material_id));
		else return false;
		if(!input || !std::isfinite(loaded_radius) || loaded_radius <= 0.0
			|| (loaded_material_id != default_materials::void_material
				&& loaded_material_id >= getMaterialCount())
			|| !std::isfinite(loaded_center.x) || !std::isfinite(loaded_center.y)
			|| !std::isfinite(loaded_center.z)) return false;
		center = loaded_center;
		radius = loaded_radius;
		material_id = loaded_material_id;
		return true;
	}

	/** @copydoc PlanetTerrainEdit::write */
	void PlanetTerrainEdit::write(std::ostream& output) const
	{
		output.write(reinterpret_cast<const char*>(&center.x), sizeof(double));
		output.write(reinterpret_cast<const char*>(&center.y), sizeof(double));
		output.write(reinterpret_cast<const char*>(&center.z), sizeof(double));
		output.write(reinterpret_cast<const char*>(&radius), sizeof(double));
		output.write(reinterpret_cast<const char*>(&material_id), sizeof(material_id));
	}
}
