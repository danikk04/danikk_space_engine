#pragma once

#include <danikk_space_engine/systems/planet/data.h>

namespace danikk_space_engine
{
	/** @brief Owns canonical planet state, settings access, terrain queries, and terrain editing. */
	class PlanetSystem
	{
		World& world;
		DynamicArray<PlanetData> planets;

	public:
		/**
		 * @brief Creates a planet subsystem bound to its owning world.
		 */
		explicit PlanetSystem(World& world) : world(world) {}
		/**
		 * @brief Logs initialization of the planet subsystem.
		 */
		void init();
		/**
		 * @brief Logs shutdown of canonical planet data.
		 */
		void shutdown();

		/**
		 * @brief Registers a planet on an existing WorldBody and loads its saved terrain edits.
		 */
		PlanetData& createPlanet(PlanetSettingsId settings_id, WorldBodyId world_body_id);
		/**
		 * @brief Removes a registered planet and asks PlanetRenderSystem to discard its cached runtime.
		 */
		void destroyPlanet(PlanetId planet_id);
		/**
		 * @brief Checks whether a planet ID currently belongs to a registered planet.
		 */
		bool isPlanet(PlanetId planet_id) const;
		/**
		 * @brief Finds a registered planet by its stable planet ID.
		 */
		PlanetData* findPlanet(PlanetId planet_id);
		/**
		 * @brief Finds a registered planet by its stable planet ID without allowing mutation.
		 */
		const PlanetData* findPlanet(PlanetId planet_id) const;
		/**
		 * @brief Finds the planet associated with a world-body ID.
		 */
		PlanetData* findPlanetByWorldBodyId(WorldBodyId world_body_id);
		/**
		 * @brief Finds the planet associated with a world-body ID without allowing mutation.
		 */
		const PlanetData* findPlanetByWorldBodyId(WorldBodyId world_body_id) const;
		/**
		 * @brief Checks whether a world-body ID identifies a planet body.
		 */
		bool isPlanetWorldBody(WorldBodyId world_body_id) const;
		/**
		 * @brief Removes the planet attached to a world body.
		 */
		void destroyPlanetForWorldBody(WorldBodyId world_body_id);
		/**
		 * @brief Returns a registered planet or raises a fatal error for an invalid ID.
		 */
		PlanetData& getPlanet(PlanetId planet_id);
		/**
		 * @brief Returns a registered planet as read-only data or raises a fatal error for an invalid ID.
		 */
		const PlanetData& getPlanet(PlanetId planet_id) const;
		/**
		 * @brief Returns all registered planets as read-only data.
		 */
		const DynamicArray<PlanetData>& getPlanets() const { return planets; }
		/** @brief Initializes and calculates the farthest planet extent and nearest surface distance from a camera position. */
		void updateRenderDistances(const glm::dvec3& camera_position, double camera_far_clip_distance, double& far_render_distance, double& closest_planet_surface_distance) const;

		/**
		 * @brief Applies and persists a spherical terrain edit on a registered planet.
		 * @param planet_id Planet to modify.
		 * @param position World-space center of the edit sphere.
		 * @param radius Edit sphere radius in metres; nonpositive values are rejected.
		 * @param material_id Fill material, or material ID zero to remove terrain.
		 * @return True when the planet was found and the edit was added.
		 */
		bool editPlanetGround(PlanetId planet_id, const glm::dvec3& position,
			double radius, MaterialId material_id);
		/**
		 * @brief Removes a terrain edit by its stable planet-local ID.
		 */
		bool removePlanetTerrainEdit(PlanetId planet_id, PlanetTerrainEditId edit_id);
		/**
		 * @brief Returns immutable settings from the planet settings catalog.
		 */
		const PlanetSettings& getPlanetSettings(PlanetSettingsId id) const;

		/**
		 * @brief Samples signed terrain density using the requested cave/edit mode.
		 */
		double getPlanetDensity(PlanetId planet_id, const glm::dvec3& position,
			PlanetTerrainMode mode) const;
		/**
		 * @brief Samples the terrain material using the requested cave/edit mode.
		 */
		MaterialId getPlanetMaterial(PlanetId planet_id, const glm::dvec3& position,
			PlanetTerrainMode mode) const;
		/**
		 * @brief Computes signed height relative to the edited planet surface.
		 */
		double getPlanetHeightAboveSurface(PlanetId planet_id, const glm::dvec3& position) const;
		/**
		 * @brief Blends the nearest planet's atmosphere color into black space by altitude.
		 */
		glm::vec3 getAtmosphereBackgroundColor(const glm::dvec3& world_position) const;
		/**
		 * @brief Samples the edited planet's radial surface distance, optionally omitting fine noise layers.
		 */
		double getPlanetSurfaceRadius(PlanetId planet_id,
			const glm::dvec3& direction,
			PlanetTerrainMode mode, double minimum_wavelength = 0.0) const;
	};

}
