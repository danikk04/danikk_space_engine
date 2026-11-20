#pragma once

#include <danikk_framework/dynamic_array.h>
#include <danikk_engine/mesh.h>
#include <danikk_space_engine/fwd.h>
#include <danikk_space_engine/id.h>
#include <danikk_space_engine/systems/chunk_mesh_generation/chunk_mesh_generation.h>

namespace danikk_space_engine
{
	namespace planet_settings_ids
	{
		inline constexpr PlanetSettingsId earth = 0;
		inline constexpr PlanetSettingsId count = 1;
	}

	inline constexpr double default_planet_radius = 6371000.0;
	inline constexpr double default_planet_core_radius = 3480000.0;
	inline constexpr double planet_chunk_load_radius = 150.0;
	inline constexpr double planet_chunk_width = 32.0;
	inline constexpr double planet_voxel_size = 2.0;
	inline constexpr int32 planet_volume_chunk_half_width = 2;

	/** @brief Immutable settings used when registering a planet for a WorldBody. */
	struct PlanetSettings
	{
		const char* name = "Earth";
		double radius = default_planet_radius;
		double core_radius = default_planet_core_radius;
		double chunk_load_radius = planet_chunk_load_radius;
		double surface_gravity = 9.8;
		/** @brief RGB color of the atmosphere near the planet surface. */
		glm::vec3 atmosphere_color = glm::vec3(0.2f, 0.4f, 0.8f);
		/** @brief Height above the surface where the atmosphere fades completely to space. */
		double atmosphere_height = 100000.0;
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
		glm::vec3 atmosphere_color = glm::vec3(0.2f, 0.4f, 0.8f);
		double atmosphere_height = 100000.0;
		uint64 noise_seed = 0;
		glm::ivec3 last_chunk_update_grid_coordinate = glm::ivec3(0);
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

	inline constexpr uint8 planet_lod_level_count = 6;
	inline constexpr uint8 planet_lod_face_count = 6;

	/** @brief Mutually exclusive render source selected for one planet. */
	enum class PlanetSurfaceRenderMode : uint8
	{
		volume_chunks,
		local_surface_lod,
		global_surface_lod
	};

	/** @brief Material range in one planetary LOD mesh. */
	struct PlanetLodMaterialRange
	{
		MaterialId material_id = 0;
		uint32 first_index = 0;
		uint32 index_count = 0;
	};

	/** @brief Generated CPU-side tile payload for a planet LOD level. */
	struct PlanetLodPatch
	{
		DynamicArray<vec3> positions;
		DynamicArray<uint32> triangle_indices;
		DynamicArray<MaterialId> triangle_material_ids;
		glm::dvec3 origin = glm::dvec3(0.0);
		glm::ivec3 grid_coordinate = glm::ivec3(0);
		bool ready = false;
	};

	/** @brief GPU mesh and pending tile data for one surface resolution. */
	struct PlanetLodLevel
	{
		Mesh* mesh = nullptr;
		DynamicArray<PlanetLodMaterialRange> material_ranges;
		DynamicArray<PlanetLodPatch> patches;
		uint32 resolution = 0;
		bool camera_local = false;
		glm::dvec3 mesh_origin = glm::dvec3(0.0);
		bool initialized = false;
	};

	/** @brief Per-planet LOD state owned and updated by PlanetSystem. */
	struct PlanetLodData
	{
		WorldBodyId world_body_id = static_cast<WorldBodyId>(-1);
		uint64 planet_generation_id = 0;
		uint64 terrain_revision = 0;
		uint8 desired_level = 0;
		uint8 active_level = 0;
		bool has_desired_level = false;
		bool has_active_level = false;
		bool enabled = false;
		bool show_local_patch = false;
		PlanetSurfaceRenderMode render_mode = PlanetSurfaceRenderMode::volume_chunks;
		uint8 render_level = 1;
		std::shared_ptr<const PlanetTerrainSnapshot> terrain_snapshot;
		DynamicArray<PlanetLodLevel> levels;
	};

	/** @brief Owns planet state and provides terrain queries, editing, streaming, and result acceptance. */
	class PlanetSystem
	{
		World& world;
		DynamicArray<PlanetData> planets;
		DynamicArray<PlanetLodData> lod_data;
		uint64 next_generation_id = 1;

		void updatePlanet(PlanetData& planet);
		PlanetLodData& getOrCreatePlanetLodData(const PlanetData& planet);
		void destroyPlanetLodMeshes(PlanetLodData& planet);
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
		/** @brief Updates LOD selection and queues missing surface meshes for each planet. */
		void updateLod();
		/** @brief Accepts a worker-generated LOD tile and uploads a complete level when ready. */
		void acceptGeneratedLodMesh(const GeneratedChunkMesh& generated_mesh);
		/** @brief Draws the single selected global surface LOD level per planet. */
		void drawLodFar() const;
		/** @brief Draws the selected camera-local surface LOD tile per planet. */
		void drawLodNear() const;
		/** @brief Returns whether volume terrain chunks are the selected surface representation. */
		bool shouldDrawPlanetChunks(WorldBodyId world_body_id) const;
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
		static double getPlanetDensity(const PlanetTerrainSnapshot& snapshot, const glm::dvec3& position);
		static MaterialId getPlanetMaterial(const PlanetTerrainSnapshot& snapshot, const glm::dvec3& position);
		/** @brief Samples the cave-free surface, optionally omitting noise layers finer than the LOD grid can represent. */
		static double getPlanetSurfaceRadius(const PlanetTerrainSnapshot& snapshot,
			const glm::dvec3& direction, double minimum_wavelength = 0.0);
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
