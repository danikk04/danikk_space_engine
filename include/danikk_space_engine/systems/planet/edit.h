#pragma once

#include <iosfwd>

#include <glm/glm.hpp>

#include <danikk_space_engine/id.h>
#include <danikk_space_engine/resource_pool.h>

namespace danikk_space_engine
{
	/**
	 * @brief One spherical stamp that adds or removes solid terrain on a planet.
	 */
	class PlanetTerrainEdit
	{
		glm::dvec3 center = glm::dvec3(0.0);
		double radius = 0.0;
		bool adds_solid_terrain = false;

	public:
		/** @brief Constructs an edit stamp from its planet-local sphere and operation. */
		PlanetTerrainEdit() = default;
		PlanetTerrainEdit(const glm::dvec3& center, double radius, bool adds_solid_terrain);

		/** @brief Stamp center in coordinates relative to the planet center. */
		double applyToScalarField(const glm::dvec3& position, double density) const;
		/** @brief Applies this stamp's radial intersection to a sampled surface radius. */
		double applyToSurfaceRadius(const glm::dvec3& direction, double surface_radius) const;
		/** @brief Returns the farthest radial extent contributed by this stamp. */
		double getMaximumRadiusFromOrigin(double margin) const;
		/** @brief Tests whether this stamp intersects an expanded axis-aligned chunk box. */
		bool intersectsAabb(const glm::dvec3& minimum, const glm::dvec3& maximum,
			double expansion) const;
		/** @brief Reads and validates one serialized edit record. */
		bool read(std::istream& input);
		/** @brief Writes this edit in the terrain-save record format. */
		void write(std::ostream& output) const;
	};

	using PlanetTerrainEditPool = WorldResourcePool<PlanetTerrainEdit, PlanetTerrainEditId>;
}
