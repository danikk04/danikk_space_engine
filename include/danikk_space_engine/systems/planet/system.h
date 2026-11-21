#pragma once

#include <danikk_space_engine/systems/planet/data.h>

namespace danikk_space_engine
{
	/** @brief Owns planet state and provides terrain queries, editing, streaming, and result acceptance. */
	class PlanetSystem
	{
		World& world;
		DynamicArray<PlanetData> planets;
		uint64 next_generation_id = 1;

		void updatePlanet(PlanetData& planet);
		void updatePlanetSurfaceTree(PlanetData& planet, const glm::dvec3& camera_position,
			double height);
		void destroyPlanetSurfaceTree(PlanetData& planet);
		void updatePlanetSpatialChunks(PlanetData& planet, const glm::dvec3& chunk_center_position);
		void queuePlanetRequests(PlanetData& planet);
		void unloadChunksOutsideDesiredSet(PlanetData& planet);
		void removeQueuedRequests(const PlanetData& planet);

	public:
		explicit PlanetSystem(World& world) : world(world) {}
		/** @brief Logs subsystem initialization. */
		void init();
		/** @brief Logs subsystem shutdown. */
		void shutdown();

		PlanetData& createPlanet(WorldBodyId world_body_id, PlanetSettingsId settings_id);
		void destroyPlanet(WorldBodyId world_body_id);
		bool isPlanet(WorldBodyId world_body_id) const;
		PlanetData* findPlanet(WorldBodyId world_body_id);
		const PlanetData* findPlanet(WorldBodyId world_body_id) const;
		PlanetData& getPlanet(WorldBodyId world_body_id);
		const PlanetData& getPlanet(WorldBodyId world_body_id) const;
		const DynamicArray<PlanetData>& getPlanets() const { return planets; }

		/** @brief Updates planet chunk streaming once for the current world frame. */
		void update();
		void acceptGeneratedSurfaceNode(const GeneratedChunkMesh& generated_mesh);
		/**
		 * @brief Applies and persists a spherical terrain edit on a registered planet.
		 * @param world_body_id Registered planet body to modify.
		 * @param position World-space center of the edit sphere.
		 * @param radius Edit sphere radius in metres; nonpositive values are rejected.
		 * @param material_id Fill material, or material ID zero to remove terrain.
		 * @return True when the planet was found and the edit was added.
		 */
		bool editPlanetGround(WorldBodyId world_body_id, const glm::dvec3& position,
			double radius, MaterialId material_id);
		/** @brief Removes a terrain edit by its stable planet-local ID. */
		bool removePlanetTerrainEdit(WorldBodyId world_body_id, PlanetTerrainEditId edit_id);
		/** @brief Returns immutable settings from the planet settings catalog. */
		const PlanetSettings& getPlanetSettings(PlanetSettingsId id) const;

		double getPlanetDensity(WorldBodyId world_body_id, const glm::dvec3& position,
			PlanetTerrainMode mode) const;
		MaterialId getPlanetMaterial(WorldBodyId world_body_id, const glm::dvec3& position,
			PlanetTerrainMode mode) const;
		double getPlanetHeightAboveSurface(WorldBodyId world_body_id, const glm::dvec3& position) const;
		/** @brief Blends the nearest planet's atmosphere color into black space by altitude. */
		glm::vec3 getAtmosphereBackgroundColor(const glm::dvec3& world_position) const;
		/** @brief Samples the cave-free surface from current planet data, optionally omitting fine noise layers. */
		double getPlanetSurfaceRadius(WorldBodyId world_body_id,
			const glm::dvec3& direction,
			PlanetTerrainMode mode, double minimum_wavelength = 0.0) const;
	};

}
