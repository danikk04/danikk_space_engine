#pragma once

#include <danikk_space_engine/default.h>
#include <danikk_framework/dynamic_array.h>
#include <danikk_space_engine/material.h>
#include <glm/glm.hpp>

namespace danikk_space_engine
{

	/**
	 * @brief Stores a persistent spherical terrain edit with position, radius, and add/remove operation.
	 */
	struct PlanetTerrainEdit
	{
		/** @brief Center of the spherical terrain stamp in coordinates relative to the planet center. */
		glm::dvec3 center = glm::dvec3(0.0);
		/** @brief Stamp radius in world units. */
		double radius = 0.0;
		/** @brief True adds solid terrain; false subtracts a spherical region from the field. */
		bool adds_solid_terrain = false;
	};

	/**
	 * @brief Samples the signed density field used to generate and collide with a planet.
	 *
	 * Positive values represent solid material, negative values represent air, and zero is the
	 * isosurface. The result combines the radial shell, deterministic noise, terrain edit stamps,
	 * and the solid core boundary. Cave density is combined with the shell using a minimum operation;
	 * additive/subtractive edits use max/min sphere fields, then the noisy core is unioned last so it
	 * cannot be excavated by caves or saved edits.
	 * @param position_relative_to_planet_center Sample position in planet-centered world units.
	 * @param radius Nominal outer radius of the planet.
	 * @param noise_seed Stable seed selecting the procedural terrain pattern.
	 * @param edits Additive/subtractive spherical stamps applied to the base field.
	 * @param core_radius Radius of the core region kept solid by the field.
	 * @return Signed density; values greater than zero are inside solid terrain.
	 */
	double samplePlanetScalarField(
		const glm::dvec3& position_relative_to_planet_center,
		double radius,
		uint64 noise_seed,
		const DynamicArray<PlanetTerrainEdit>& edits,
		double core_radius);

	/**
	 * @brief Finds the first density sign change on a finite planet-local ray.
	 * @param origin Ray origin relative to the planet center.
	 * @param direction Ray direction in planet-local coordinates; normalized internally.
	 * @param max_distance Maximum search distance in metres.
	 * @param radius Nominal planet radius.
	 * @param noise_seed Terrain noise seed.
	 * @param edits Terrain edits evaluated in their existing order.
	 * @param core_radius Radius of the solid core.
	 * @param distance Receives the distance from the origin to the detected surface.
	 * @param normal Receives the outward unit normal in planet-local coordinates.
	 * @return True when a transition is found and its normal is valid.
	 * Uses half-metre samples and refines a detected transition by bisection. Features
	 * thinner than the sampling step can be missed when both boundaries lie between samples.
	 */
	bool raycastPlanetScalarField(const glm::dvec3& origin, const glm::dvec3& direction,
		double max_distance, double radius, uint64 noise_seed,
		const DynamicArray<PlanetTerrainEdit>& edits, double core_radius,
		double& distance, glm::dvec3& normal);

	/**
	 * @brief Selects a terrain material for a point on or near the procedural surface.
	 * @param position_relative_to_planet_center Position relative to the planet center.
	 * @param radius Nominal outer radius used to estimate surface height.
	 * @param noise_seed Seed shared with terrain generation.
	 * @return Grass for samples at or above the three-metre subsurface boundary,
	 *         otherwise stone; the exact center is stone. Callers use this at
	 *         surface triangle centers, so exterior samples are also classified as grass.
	 */
	MaterialId getPlanetSurfaceMaterial(
		const glm::dvec3& position_relative_to_planet_center,
		double radius,
		uint64 noise_seed);
}
