#pragma once

#include <array>
#include <cmath>
#include <memory>
#include <limits>

#include <danikk_framework/array.h>
#include <danikk_framework/dynamic_array.h>
#include <danikk_engine/mesh.h>
#include <danikk_space_engine/fwd.h>
#include <danikk_space_engine/id.h>
#include <danikk_space_engine/systems/planet/edit.h>
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
	inline constexpr int32 planet_volume_chunk_half_width = 6;
	inline constexpr uint32 planet_lod_triangle_budget = 100000;
	inline constexpr uint32 planet_far_lod_triangle_budget = 500;

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

	/** @brief Controls which generated and edited terrain fields a planet query samples. */
	enum class PlanetTerrainMode : uint8
	{
		heightmap, ///< Procedural surface only; excludes caves and terrain edits.
		heightmap_with_edits, ///< Procedural surface plus terrain edits; excludes caves.
		cave_field, ///< Procedural terrain with caves; excludes terrain edits.
		cave_field_with_edits ///< Full volumetric terrain with caves and terrain edits.
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

	/** @brief Edit handle and stable application order stored in a spatial bucket. */
	struct PlanetTerrainEditReference
	{
		PlanetTerrainEditId edit_id = 0;
		uint64 order = 0;
	};

	/** @brief Hash bucket of edits whose centers occupy one logical planet chunk. */
	struct PlanetTerrainEditBucket
	{
		glm::ivec3 grid_coordinate = glm::ivec3(0);
		DynamicArray<PlanetTerrainEditReference> edits;
		uint8 state = 0;
	};

	/** @brief Spatial index for local edits and a separate list of planet-scale edits. */
	struct PlanetTerrainEditIndex
	{
		DynamicArray<PlanetTerrainEditBucket> buckets;
		DynamicArray<PlanetTerrainEditReference> large_edits;
		size_t bucket_count = 0;
		size_t used_bucket_count = 0;
		uint64 next_order = 0;

		void insert(PlanetTerrainEditId edit_id, const PlanetTerrainEdit& edit, double chunk_width);
		bool remove(PlanetTerrainEditId edit_id, const PlanetTerrainEdit& edit, double chunk_width);
		void clear();
		size_t findBucket(const glm::ivec3& coordinate) const;

		/** @brief Visits nearby local edits and large edits in original application order. */
		template<typename callback_t>
		void forEachCandidate(const glm::dvec3& position, double chunk_width, callback_t&& callback) const
		{
			if(chunk_width <= 0.0) return;
			const glm::ivec3 center(
				static_cast<int32>(std::floor(position.x / chunk_width)),
				static_cast<int32>(std::floor(position.y / chunk_width)),
				static_cast<int32>(std::floor(position.z / chunk_width)));
			std::array<const DynamicArray<PlanetTerrainEditReference>*, 28> sources{};
			size_t source_count = 0;
			for(int32 z = -1; z <= 1; ++z)
				for(int32 y = -1; y <= 1; ++y)
					for(int32 x = -1; x <= 1; ++x)
					{
						const size_t index = findBucket(center + glm::ivec3(x, y, z));
						if(index != static_cast<size_t>(-1) && buckets[index].edits.size() > 0)
							sources[source_count++] = &buckets[index].edits;
					}
			if(large_edits.size() > 0) sources[source_count++] = &large_edits;
			std::array<size_t, 28> cursors{};
			while(true)
			{
				size_t best_source = source_count;
				uint64 best_order = std::numeric_limits<uint64>::max();
				for(size_t source = 0; source < source_count; ++source)
				{
					const auto& edits = *sources[source];
					if(cursors[source] >= edits.size()) continue;
					if(edits[cursors[source]].order < best_order)
					{
						best_order = edits[cursors[source]].order;
						best_source = source;
					}
				}
				if(best_source == source_count) break;
				callback((*sources[best_source])[cursors[best_source]++].edit_id);
			}
		}
	};

	/** @brief Owns a planet's terrain edits and their chronological and spatial indexes. */
	struct PlanetTerrainEdits
	{
		std::shared_ptr<PlanetTerrainEditPool> pool =
			std::make_shared<PlanetTerrainEditPool>("PlanetTerrainEdit");
		DynamicArray<PlanetTerrainEditId> ids;
		PlanetTerrainEditIndex index;

		/** @brief Adds an edit to storage and both indexes in application order. */
		PlanetTerrainEditId append(const PlanetTerrainEdit& edit, double chunk_width)
		{
			const PlanetTerrainEditId edit_id = pool->create();
			(*pool)[edit_id] = edit;
			ids.push(edit_id);
			index.insert(edit_id, edit, chunk_width);
			return edit_id;
		}
	};

	inline constexpr uint8 planet_lod_level_count = 5;
	inline constexpr uint8 planet_chunk_level_count = planet_lod_level_count + 1;
	inline constexpr uint8 planet_lod_face_count = 6;
	inline constexpr uint16 planet_local_heightmap_tile_radius = 6;
	inline constexpr uint16 planet_local_heightmap_tile_side = planet_local_heightmap_tile_radius * 2 + 1;
	inline constexpr uint16 planet_local_heightmap_tile_count =
		planet_local_heightmap_tile_side * planet_local_heightmap_tile_side;

	/** @brief Geometry source represented by a planet render level. */
	enum class PlanetChunkLevelGeometry : uint8
	{
		regular_volume_chunks,
		local_heightmap,
		global_heightmap
	};

	/** @brief Rule used to size the surface represented by a planet LOD level. */
	enum class PlanetLodCoverage : uint8
	{
		regular_chunks,
		fixed_diameter,
		horizon,
		whole_planet
	};

	/** @brief All generation and transition parameters for one planet LOD level. */
	struct PlanetLodSettings
	{
		PlanetChunkLevelGeometry geometry;
		PlanetLodCoverage coverage;
		double activation_height;
		double deactivation_height;
		double activation_radius_multiplier;
		double deactivation_radius_multiplier;
		/** @brief Diameter in fixed_diameter mode; target horizon altitude in horizon mode. */
		double coverage_parameter;
		uint32 resolution;
		uint32 patch_count;
		uint32 triangle_budget;

		constexpr double getActivationHeight(double planet_radius) const
		{
			return activation_height + activation_radius_multiplier * planet_radius;
		}

		constexpr double getDeactivationHeight(double planet_radius) const
		{
			return deactivation_height + deactivation_radius_multiplier * planet_radius;
		}
	};

	/** @brief Per-level LOD tuning table, indexed by the rendered level (including level zero). */
	inline const danikk_framework::Array<PlanetLodSettings, planet_chunk_level_count> planet_lod_settings = {
		PlanetLodSettings{PlanetChunkLevelGeometry::regular_volume_chunks, PlanetLodCoverage::regular_chunks,
			0.0, 0.0, 0.0, 0.0, 0.0, 0, 0, 0},
		PlanetLodSettings{PlanetChunkLevelGeometry::local_heightmap, PlanetLodCoverage::fixed_diameter,
			100.0, 70.0, 0.0, 0.0, 20000.0, 16, planet_local_heightmap_tile_count,
			planet_lod_triangle_budget},
		PlanetLodSettings{PlanetChunkLevelGeometry::local_heightmap, PlanetLodCoverage::horizon,
			1000.0, 700.0, 0.0, 0.0, 10000.0, 16, planet_local_heightmap_tile_count,
			planet_lod_triangle_budget},
		PlanetLodSettings{PlanetChunkLevelGeometry::local_heightmap, PlanetLodCoverage::horizon,
			10000.0, 7000.0, 0.0, 0.0, 100000.0, 16, planet_local_heightmap_tile_count,
			planet_lod_triangle_budget},
		PlanetLodSettings{PlanetChunkLevelGeometry::global_heightmap, PlanetLodCoverage::whole_planet,
			100000.0, 70000.0, 0.0, 0.0, 0.0, 91, planet_lod_face_count,
			planet_lod_triangle_budget},
		PlanetLodSettings{PlanetChunkLevelGeometry::global_heightmap, PlanetLodCoverage::whole_planet,
			0.0, 0.0, 20.0, 14.0, 0.0, 6, planet_lod_face_count,
			planet_far_lod_triangle_budget}
	};

	inline const PlanetLodSettings& getPlanetLodSettings(uint8 level)
	{
		return planet_lod_settings.data()[level];
	}

	inline uint8 findFirstGlobalPlanetLodLevel()
	{
		for(uint8 level = 1; level < planet_chunk_level_count; level++)
			if(getPlanetLodSettings(level).geometry == PlanetChunkLevelGeometry::global_heightmap)
				return level;
		return planet_chunk_level_count;
	}

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

	/** @brief Render resources for one planet level; level zero references regular chunks. */
	struct PlanetChunkLevel
	{
		/** @brief Geometry generation strategy used by this level. */
		PlanetChunkLevelGeometry geometry = PlanetChunkLevelGeometry::regular_volume_chunks;
		/** @brief Regular chunk IDs for level zero; unused by LOD levels. */
		DynamicArray<ChunkId> chunk_ids;
		/** @brief Combined surface mesh; unused by the regular chunk level. */
		Mesh* mesh = nullptr;
		DynamicArray<PlanetLodMaterialRange> material_ranges;
		DynamicArray<PlanetLodPatch> patches;
		/** @brief Number of mesh patches required before this level can be rendered. */
		uint32 expected_patch_count = 0;
		uint32 resolution = 0;
		bool camera_local = false;
		bool grid_center_set = false;
		glm::ivec3 grid_center = glm::ivec3(0);
		glm::dvec3 heightmap_right = glm::dvec3(1.0, 0.0, 0.0);
		glm::dvec3 heightmap_forward = glm::dvec3(0.0, 1.0, 0.0);
		glm::dvec3 heightmap_plane_origin = glm::dvec3(0.0);
		glm::dvec3 mesh_origin = glm::dvec3(0.0);
		/** @brief Origin used by the currently uploaded mesh while a replacement is generated. */
		glm::dvec3 render_mesh_origin = glm::dvec3(0.0);
		bool initialized = false;
	};

	/** @brief Terrain, streaming, and render state owned by one planet. */
	struct PlanetData
	{
		WorldBodyId world_body_id = static_cast<WorldBodyId>(-1);
		PlanetSettingsId settings_id = planet_settings_ids::earth;
		uint64 generation_id = 0;
		glm::ivec3 last_chunk_update_grid_coordinate = glm::ivec3(0);
		uint64 terrain_revision = 0;
		uint64 lod_terrain_revision = 0;
		uint64 last_chunk_update_revision = 0;
		PlanetMeshGenerator last_chunk_update_mesh_generator = PlanetMeshGenerator::marching_tetrahedra;
		size_t processed_edit_count = 0;
		bool terrain_edits_loaded = false;
		bool chunk_set_initialized = false;
		bool chunk_updates_active = false;
		/** @brief True while ordinary chunk requests are paused in favor of a surface LOD. */
		bool chunk_requests_paused_for_lod = false;
		PlanetTerrainEdits terrain_edits;
		PlanetChunkLookup chunk_lookup;
		DynamicArray<ChunkMeshGenerationRequest> pending_requests;
		/** @brief Current representation: zero uses ordinary chunks, positive values select one LOD. */
		uint8 lod_level = 0;
		/** @brief Desired LOD independent of the currently displayed fallback level. */
		uint8 target_lod_level = 0;
		bool lod_enabled = false;
		DynamicArray<PlanetChunkLevel> levels;

	};
}
