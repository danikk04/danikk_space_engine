#pragma once

#include <danikk_framework/dynamic_array.h>
#include <danikk_space_engine/fwd.h>
#include <danikk_space_engine/id.h>
#include <danikk_space_engine/chunk_mesh_generator.h>

namespace danikk_space_engine
{
	namespace planet_settings_ids
	{
		inline constexpr PlanetSettingsId earth = 0;
		inline constexpr PlanetSettingsId count = 1;
	}

	inline constexpr double default_planet_radius = 6371000.0;
	inline constexpr double default_planet_core_radius = 3480000.0;
	inline constexpr double planet_chunk_load_radius = 100.0;
	inline constexpr double planet_chunk_width = 32.0;
	inline constexpr double planet_voxel_size = 2.0;

	/** @brief Immutable settings used when registering a planet for a WorldBody. */
	struct PlanetSettings
	{
		const char* name = "Earth";
		double radius = default_planet_radius;
		double core_radius = default_planet_core_radius;
		double chunk_load_radius = planet_chunk_load_radius;
		double surface_gravity = 9.8;
		uint64 noise_seed = 0;
	};

	struct PlanetChunkLookupEntry
	{
		glm::ivec3 grid_coordinate = glm::ivec3(0);
		ChunkId chunk_id = static_cast<ChunkId>(-1);
		uint8 state = 0;
		bool desired = false;
		bool has_record = false;
		PlanetMeshGenerator mesh_generator = PlanetMeshGenerator::marching_tetrahedra;
	};

	/** @brief Per-planet hash lookup of desired, pending, generated, and loaded chunks. */
	struct PlanetChunkLookup
	{
		DynamicArray<PlanetChunkLookupEntry> entries;
		size_t entry_count = 0;
		size_t used_bucket_count = 0;

		void clearDesired();
		void markDesired(const glm::ivec3& grid_coordinate);
		void setChunk(const glm::ivec3& grid_coordinate, ChunkId chunk_id,
			PlanetMeshGenerator mesh_generator = PlanetMeshGenerator::marching_tetrahedra);
		void remove(const glm::ivec3& grid_coordinate);
		bool isDesired(const glm::ivec3& grid_coordinate) const;
		bool isGenerated(const glm::ivec3& grid_coordinate, PlanetMeshGenerator mesh_generator) const;
		bool getChunk(const glm::ivec3& grid_coordinate, ChunkId& chunk_id) const;
	};

	/** @brief All runtime state for one planet, indexed by its owning WorldBodyId. */
	struct PlanetData
	{
		WorldBodyId world_body_id = static_cast<WorldBodyId>(-1);
		uint64 generation_id = 0;
		const char* name = "Earth";
		double radius = default_planet_radius;
		double core_radius = default_planet_core_radius;
		double chunk_load_radius = planet_chunk_load_radius;
		double surface_gravity = 9.8;
		uint64 noise_seed = 0;
		glm::dvec3 last_chunk_update_position = glm::dvec3(0.0);
		uint64 terrain_revision = 0;
		uint64 last_chunk_update_revision = 0;
		PlanetMeshGenerator last_chunk_update_mesh_generator = PlanetMeshGenerator::marching_tetrahedra;
		size_t processed_edit_count = 0;
		bool terrain_edits_loaded = false;
		bool chunk_set_initialized = false;
		bool chunk_updates_active = false;
		DynamicArray<PlanetTerrainEdit> terrain_edits;
		std::shared_ptr<const PlanetTerrainSnapshot> terrain_snapshot;
		PlanetChunkLookup chunk_lookup;
		DynamicArray<ChunkMeshGenerationRequest> pending_requests;
	};

	/** @brief Owns planet state and provides terrain queries, editing, streaming, and result acceptance. */
	class PlanetSystem
	{
		World& world;
		DynamicArray<PlanetData> planets;
		uint64 next_generation_id = 1;

		void updatePlanet(PlanetData& planet);
		void updatePlanetChunks(PlanetData& planet, const glm::dvec3& camera_position);
		void queuePlanetRequests(PlanetData& planet);
		void unloadChunksOutsideDesiredSet(PlanetData& planet);
		void invalidateChunksAroundEdit(PlanetData& planet, const PlanetTerrainEdit& edit);
		void removeQueuedRequests(const PlanetData& planet);

	public:
		explicit PlanetSystem(World& world) : world(world) {}

		PlanetData& createPlanet(WorldBodyId world_body_id, const PlanetSettings& settings);
		void destroyPlanet(WorldBodyId world_body_id);
		bool isPlanet(WorldBodyId world_body_id) const;
		PlanetData* findPlanet(WorldBodyId world_body_id);
		const PlanetData* findPlanet(WorldBodyId world_body_id) const;
		PlanetData& getPlanet(WorldBodyId world_body_id);
		const PlanetData& getPlanet(WorldBodyId world_body_id) const;
		const DynamicArray<PlanetData>& getPlanets() const { return planets; }

		void frame();
		void acceptGeneratedChunk(const GeneratedChunkMesh& generated_chunk);
		bool editGround(WorldBodyId world_body_id, const glm::dvec3& position,
			double radius, bool adds_solid_terrain);

		double getPlanetDensity(WorldBodyId world_body_id, const glm::dvec3& position) const;
		MaterialId getPlanetMaterial(WorldBodyId world_body_id, const glm::dvec3& position) const;
		double getPlanetHeightAboveSurface(WorldBodyId world_body_id, const glm::dvec3& position) const;
		static double getPlanetDensity(const PlanetTerrainSnapshot& snapshot, const glm::dvec3& position);
		static MaterialId getPlanetMaterial(const PlanetTerrainSnapshot& snapshot, const glm::dvec3& position);
	};

	/** @brief Applies a terrain edit to a planet registered in `world.planets`. */
	bool editPlanetGround(World& world, WorldBodyId world_body_id, const glm::dvec3& position,
		double radius, bool adds_solid_terrain);

	/** @brief Loads saved terrain stamps for a planet body and procedural seed. */
	bool loadPlanetTerrainEdits(WorldBodyId world_body_id, double planet_radius,
		uint64 noise_seed, DynamicArray<PlanetTerrainEdit>& edits, uint64& terrain_revision);

	/** @brief Persists the ordered terrain stamps for a planet body. */
	void savePlanetTerrainEdits(WorldBodyId world_body_id, double planet_radius,
		uint64 noise_seed, const DynamicArray<PlanetTerrainEdit>& edits, uint64 terrain_revision);

	const PlanetSettings& getPlanetSettings(PlanetSettingsId id);
	bool raycastPlanetScalarField(const World& world, WorldBodyId planet_body_id,
		const glm::dvec3& origin, const glm::dvec3& direction, double max_distance,
		double& distance, glm::dvec3& normal);
}
