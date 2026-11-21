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


	/** @brief Terrain, streaming, and render state owned by one planet. */
	struct PlanetData
	{
		WorldBodyId world_body_id = static_cast<WorldBodyId>(-1);
		PlanetSettingsId settings_id = planet_settings_ids::earth;
		uint64 generation_id = 0;
		glm::ivec3 last_chunk_update_grid_coordinate = glm::ivec3(0);
		uint64 terrain_revision = 0;
		uint64 last_chunk_update_revision = 0;
		PlanetMeshGenerator last_chunk_update_mesh_generator = PlanetMeshGenerator::marching_tetrahedra;
		size_t processed_edit_count = 0;
		bool terrain_edits_loaded = false;
		bool chunk_set_initialized = false;
		bool chunk_updates_active = false;
		PlanetTerrainEdits terrain_edits;
		PlanetChunkLookup chunk_lookup;
		DynamicArray<ChunkMeshGenerationRequest> pending_requests;

	};
}
