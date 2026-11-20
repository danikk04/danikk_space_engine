#pragma once

#include <danikk_space_engine/systems/planet/data.h>

namespace danikk_space_engine
{
	/** @brief Planet LOD rendering phase selected by World for the current depth pass. */
	enum class PlanetRenderPass : uint8
	{
		all,
		far_depth,
		near_depth
	};

	/** @brief Owns planet state and provides terrain queries, editing, streaming, and result acceptance. */
	class PlanetSystem
	{
		World& world;
		DynamicArray<PlanetData> planets;
		uint64 next_generation_id = 1;

		void updatePlanet(PlanetData& planet);
		void updateLod();
		void initializePlanetLodLevels(PlanetData& planet);
		void destroyPlanetLodMeshes(PlanetData& planet);
		void updatePlanetChunks(PlanetData& planet, const glm::dvec3& camera_position,
			const glm::dvec3& chunk_center_position);
		void queuePlanetRequests(PlanetData& planet);
		void unloadChunksOutsideDesiredSet(PlanetData& planet);
		void invalidateChunksAroundEdit(PlanetData& planet, const PlanetTerrainEdit& edit);
		void removeQueuedRequests(const PlanetData& planet);

	public:
		explicit PlanetSystem(World& world) : world(world) {}
		/** @brief Clears cached LOD meshes and logs subsystem initialization. */
		void init();
		/** @brief Cancels LOD work, clears cached meshes, and logs shutdown. */
		void shutdown();

		PlanetData& createPlanet(WorldBodyId world_body_id, PlanetSettingsId settings_id);
		void destroyPlanet(WorldBodyId world_body_id);
		bool isPlanet(WorldBodyId world_body_id) const;
		PlanetData* findPlanet(WorldBodyId world_body_id);
		const PlanetData* findPlanet(WorldBodyId world_body_id) const;
		PlanetData& getPlanet(WorldBodyId world_body_id);
		const PlanetData& getPlanet(WorldBodyId world_body_id) const;
		const DynamicArray<PlanetData>& getPlanets() const { return planets; }

		/** @brief Updates planet streaming and LOD selection once for the current world frame. */
		void update();
		/** @brief Draws the selected planet LODs for the specified World depth pass. */
		void frame(PlanetRenderPass pass) const;
		/** @brief Accepts a worker-generated LOD tile and uploads a complete level when ready. */
		void acceptGeneratedLodMesh(const GeneratedChunkMesh& generated_mesh);
		/** @brief Returns whether volume terrain chunks are the selected surface representation. */
		bool shouldDrawPlanetChunks(WorldBodyId world_body_id) const;
		/** @brief Gets the selected render level for a planet, returning false if it has no render state yet. */
		bool getCurrentLodLevel(WorldBodyId world_body_id, uint8& lod_level) const;
		/** @brief Returns the regular chunk IDs stored at render level zero, or null if unavailable. */
		const DynamicArray<ChunkId>* getRegularChunkIds(WorldBodyId world_body_id) const;
		/** @brief Cancels pending LOD work and releases all planet surface meshes. */
		void clearLod();
		void acceptGeneratedChunk(const GeneratedChunkMesh& generated_chunk);
		bool editGround(WorldBodyId world_body_id, const glm::dvec3& position,
			double radius, bool adds_solid_terrain);

		double getPlanetDensity(WorldBodyId world_body_id, const glm::dvec3& position) const;
		MaterialId getPlanetMaterial(WorldBodyId world_body_id, const glm::dvec3& position) const;
		double getPlanetHeightAboveSurface(WorldBodyId world_body_id, const glm::dvec3& position) const;
		/** @brief Blends the nearest planet's atmosphere color into black space by altitude. */
		glm::vec3 getAtmosphereBackgroundColor(const glm::dvec3& world_position) const;
		/** @brief Samples the cave-free surface from current planet data, optionally omitting fine noise layers. */
		double getPlanetSurfaceRadius(WorldBodyId world_body_id,
			const glm::dvec3& direction, double minimum_wavelength = 0.0) const;
	};

	/** @brief Applies a terrain edit to a planet registered in `world.planets`. */
	bool editPlanetGround(World& world, WorldBodyId world_body_id, const glm::dvec3& position,
		double radius, bool adds_solid_terrain);

	/** @brief Loads saved terrain stamps for a planet body and procedural seed. */
	bool loadPlanetTerrainEdits(WorldBodyId world_body_id, double planet_radius,
		uint64 noise_seed, PlanetTerrainEditPool& edit_pool,
		DynamicArray<PlanetTerrainEditId>& edit_ids, uint64& terrain_revision);

	/** @brief Persists the ordered terrain stamps for a planet body. */
	void savePlanetTerrainEdits(WorldBodyId world_body_id, double planet_radius,
		uint64 noise_seed, const PlanetTerrainEditPool& edit_pool,
		const DynamicArray<PlanetTerrainEditId>& edit_ids, uint64 terrain_revision);

	const PlanetSettings& getPlanetSettings(PlanetSettingsId id);
	bool raycastPlanetScalarField(const World& world, WorldBodyId planet_body_id,
		const glm::dvec3& origin, const glm::dvec3& direction, double max_distance,
		double& distance, glm::dvec3& normal);
}
