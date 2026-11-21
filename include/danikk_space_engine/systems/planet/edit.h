#pragma once

#include <iosfwd>

#include <glm/glm.hpp>

#include <danikk_space_engine/id.h>
#include <danikk_space_engine/resource_pool.h>

namespace danikk_space_engine
{
	/**
	 * @brief One spherical terrain stamp whose material selects fill or cut behavior.
	 * Material ID zero removes solid density; any other material ID adds solid terrain
	 * carrying that material. A cut preserves the material of exposed solid surfaces.
	 */
	class PlanetTerrainEdit
	{
		glm::dvec3 center = glm::dvec3(0.0);
		double radius = 0.0;
		MaterialId material_id = 0;

	public:
		/** @brief Constructs a stamp; material ID zero cuts terrain and other IDs add that material. */
		PlanetTerrainEdit() = default;
		PlanetTerrainEdit(const glm::dvec3& center, double radius, MaterialId material_id);

		const glm::dvec3& getCenter() const { return center; }
		double getRadius() const { return radius; }
		/** @brief Applies this stamp's material-derived add/cut operation to the density field. */
		double applyToScalarField(const glm::dvec3& position, double density) const;
		/** @brief Applies this stamp's radial intersection to a sampled surface radius. */
		double applyToSurfaceRadius(const glm::dvec3& direction, double surface_radius) const;
		/** @brief Applies this stamp's material inside an additive stamp; cuts preserve existing material. */
		MaterialId applyToMaterial(const glm::dvec3& position, MaterialId current_material) const;
		/** @brief Returns the farthest radial extent contributed by this stamp. */
		double getMaximumRadiusFromOrigin(double margin) const;
		/** @brief Tests whether this stamp intersects an expanded axis-aligned chunk box. */
		bool intersectsAabb(const glm::dvec3& minimum, const glm::dvec3& maximum,
			double expansion) const;
		/** @brief Reads and validates one serialized edit record in the given save format. */
		bool read(std::istream& input, uint32 format_version);
		/** @brief Writes this edit in the terrain-save record format. */
		void write(std::ostream& output) const;
	};

	using PlanetTerrainEditPool = WorldResourcePool<PlanetTerrainEdit, PlanetTerrainEditId>;
}
