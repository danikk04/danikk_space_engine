#pragma once

#include <danikk_framework/dynamic_array.h>
#include <danikk_space_engine/fwd.h>
#include <danikk_space_engine/id.h>
#include <danikk_space_engine/component/world_body/world_body.h>
#include <danikk_space_engine/component/world_body/planet/chunk_generator.h>

namespace danikk_space_engine
{
	namespace planet_settings_ids
	{
		/** @brief Settings-table index for the built-in Earth preset. */
		inline constexpr PlanetSettingsId earth = 0;
		/** @brief Number of built-in planet presets, used as the settings-table bound. */
		inline constexpr PlanetSettingsId count = 1;
	}

	/** @brief Earth-like mean surface radius, in world units (metres). */
	inline constexpr double default_planet_radius = 6371000.0;
	/** @brief Earth-like core-boundary radius, in world units (metres). */
	inline constexpr double default_planet_core_radius = 3480000.0;
	/** @brief Camera-centered radius used to request planet surface chunks. */
	inline constexpr double planet_chunk_load_radius = 100.0;
	/** @brief Edge length of a planet chunk's local generation region, in world units. */
	inline constexpr double planet_chunk_width = 32.0;
	/** @brief Distance between adjacent scalar-field samples, in world units. */
	inline constexpr double planet_voxel_size = 4.0;

	/**
	 * @brief Stores a named planet preset including radii, gravity, and terrain-generation seed.
	 */
	struct PlanetComponentSettings
	{
		/** @brief Human-readable preset label. */
		const char* name = "Earth";
		/** @brief Mean solid-surface radius in world units. */
		double radius = default_planet_radius;
		/** @brief Mean radius of the solid core boundary in world units. */
		double core_radius = default_planet_core_radius;
		/** @brief Camera-centered distance within which surface chunks are requested. */
		double chunk_load_radius = planet_chunk_load_radius;
		/** @brief Gravitational acceleration magnitude at the nominal surface. */
		double surface_gravity = 9.8;
		/** @brief Seed used to make terrain noise deterministic for this preset. */
		uint64 noise_seed = 0;
	};

	/**
	 * @brief Maps a planet grid coordinate to its chunk ID and desired/generated state.
	 *
	 * `state` is the hash bucket marker: 0 is empty, 1 is occupied, and 2 is a tombstone left
	 * by removal so linear-probe searches can continue through the collision chain. An occupied
	 * entry can be desired but not generated, generated with no geometry, or generated with a
	 * loaded chunk; `has_record` distinguishes the first state from the latter two.
	 */
	struct PlanetChunkLookupEntry
	{
		/** @brief Integer coordinate of this chunk in the planet's cube-face grid. */
		glm::ivec3 grid_coordinate = glm::ivec3(0);
		/** @brief Loaded WorldData chunk ID, or invalid ID when this coordinate is known-empty/pending. */
		ChunkId chunk_id = static_cast<ChunkId>(-1);
		/** @brief Hash-table bucket marker: 0 empty, 1 occupied, 2 deleted/tombstone. */
		uint8 state = 0;
		/** @brief Whether the coordinate belongs to the current camera-centered load set. */
		bool desired = false;
		/** @brief Whether generation completed; true even when the chunk contains no triangles. */
		bool has_record = false;
	};

	/**
	 * @brief Maps planet grid coordinates to desired, pending, generated, and loaded chunk records.
	 *
	 * Uses open addressing with linear probing. Bucket capacity is a power of two, allowing a
	 * bit-mask wraparound; the table grows before its used-bucket load reaches 70 percent and
	 * rehashing removes tombstones. `entry_count` counts occupied records, while
	 * `used_bucket_count` also includes tombstones for the growth threshold.
	 */
	struct PlanetChunkLookup
	{
		/** @brief Open-addressed hash table entries storing coordinate-to-chunk metadata. */
		DynamicArray<PlanetChunkLookupEntry> entries;
		/** @brief Number of occupied hash slots, including records without a loaded chunk. */
		size_t entry_count = 0;
		/** @brief Occupied plus tombstoned buckets, used to decide when to grow the table. */
		size_t used_bucket_count = 0;

		/**
		 * @brief Clears desired flags and tombstones entries that no longer own loaded chunks.
		 */
		void clearDesired();

		/**
		 * @brief Adds or updates a table record and marks the coordinate wanted this update.
		 * @param grid_coordinate Planet-grid coordinate to retain or schedule for loading.
		 */
		void markDesired(const glm::ivec3& grid_coordinate);

		/**
		 * @brief Records the loaded world chunk associated with a grid coordinate.
		 * @param grid_coordinate Planet-grid coordinate represented by the chunk.
		 * @param chunk_id Active WorldData chunk ID to associate with that coordinate.
		 */
		void setChunk(const glm::ivec3& grid_coordinate, ChunkId chunk_id);

		/**
		 * @brief Removes a loaded coordinate from the lookup table and clears its chunk record.
		 * @param grid_coordinate Coordinate whose loaded/generated state is being invalidated.
		 */
		void remove(const glm::ivec3& grid_coordinate);

		/**
		 * @brief Reports whether the grid coordinate is in the current desired chunk set.
		 * @param grid_coordinate Planet-grid coordinate to query.
		 * @return True after the coordinate has been marked desired for the current update.
		 */
		bool isDesired(const glm::ivec3& grid_coordinate) const;
		/**
		 * @brief Reports whether generation completed for the coordinate, even if it was empty.
		 * @param grid_coordinate Planet-grid coordinate to query.
		 * @return True when a generation record exists; this does not imply a loaded chunk ID.
		 */
		bool isGenerated(const glm::ivec3& grid_coordinate) const;

		/**
		 * @brief Retrieves the active chunk ID stored for a coordinate.
		 * @param grid_coordinate Planet-grid coordinate to query.
		 * @param chunk_id Receives the loaded chunk ID when one is present.
		 * @return True only when the record points to a currently loaded chunk.
		 */
		bool getChunk(const glm::ivec3& grid_coordinate, ChunkId& chunk_id) const;
	};

	/**
	 * @brief Holds a planet preset, persistent terrain edits, and the active chunk update state.
	 *
	 * The component coordinates asynchronous generation but does not own generated geometry;
	 * accepted chunks and their points/triangles are allocated through WorldData. The lookup
	 * tracks wanted, pending, generated-empty, and loaded coordinates without scanning all chunks.
	 */
	struct PlanetComponent
	{
		/** @brief Preset label copied from the selected PlanetComponentSettings entry. */
		const char* name = "Earth";
		/** @brief Mean procedural surface radius in world units. */
		double radius = default_planet_radius;
		/** @brief Core boundary radius used by the scalar field. */
		double core_radius = default_planet_core_radius;
		/** @brief Radius around the camera used to choose desired surface chunks. */
		double chunk_load_radius = planet_chunk_load_radius;
		/** @brief Surface gravitational acceleration magnitude. */
		double surface_gravity = 9.8;
		/** @brief Stable seed for procedural terrain and material sampling. */
		uint64 noise_seed = 0;
		/** @brief Camera world position used by the previous chunk-set update. */
		glm::dvec3 last_chunk_update_position = glm::dvec3(0.0);
		/** @brief Revision incremented whenever persistent terrain edits change. */
		uint64 terrain_revision = 0;
		/** @brief Terrain revision for which the last desired chunk set was computed. */
		uint64 last_chunk_update_revision = 0;
		/** @brief Number of saved terrain edits already applied to the component's loaded state. */
		size_t processed_edit_count = 0;
		/** @brief Whether persistent edit stamps have been loaded for this planet. */
		bool terrain_edits_loaded = false;
		/** @brief Whether the initial camera-centered chunk set has been built. */
		bool chunk_set_initialized = false;
		/** @brief Prevents overlapping chunk-set updates while an update is in progress. */
		bool chunk_updates_active = false;
		/** @brief Persistent add/remove stamps sampled by the planet scalar field. */
		DynamicArray<PlanetTerrainEdit> terrain_edits;
		/** @brief Hash lookup for desired, pending, generated, and loaded chunk coordinates. */
		PlanetChunkLookup chunk_lookup;
		/** @brief Requests currently associated with this planet component. */
		DynamicArray<ChunkGenerationRequest> pending_requests;
	};

	/**
	 * @brief Returns the immutable settings preset identified by ID.
	 * @param id Preset index, currently including the Earth preset.
	 * @return Static settings record; an out-of-range ID raises a fatal error.
	 */
	const PlanetComponentSettings& getPlanetSettings(PlanetSettingsId id);

	/** @brief Constructs a planet component from optional settings/persistent state. */
	void constructPlanetComponent(void* destination, const void* init_data);

	/** @brief Move-constructs the planet payload when its component slot is relocated. */
	void movePlanetComponent(void* destination, void* source);

	/** @brief Destroys planet-owned edit, lookup, and pending-request arrays. */
	void destroyPlanetComponent(void* data);

	/**
	 * @brief Loads edit stamps and reconciles desired chunks around the current camera position.
	 * @param slot Planet component slot, including WorldBody ID and generation token.
	 * @param world World that owns chunk resources and the asynchronous ChunkLoader.
	 */
	void updatePlanetComponent(WorldBodyComponentSlot& slot, World& world);

	/**
	 * @brief Validates and installs one completed chunk-generation result for this planet.
	 *
	 * Results from an obsolete terrain revision or component generation are discarded. Valid
	 * geometry is copied into WorldData resources and its grid-coordinate record is updated.
	 * @param slot Planet component slot that submitted the request.
	 * @param world World receiving generated chunk resources.
	 * @param generated_chunk Worker-produced geometry and request identity.
	 */
	void acceptPlanetGeneratedChunk(
		WorldBodyComponentSlot& slot,
		World& world,
		const GeneratedChunkData& generated_chunk);

	/**
	 * @brief Adds or removes a persistent spherical terrain stamp centered on the selected surface face.
	 * @param world World containing the selected face and owning planet component.
	 * @param face Stable reference to the selected ground triangle.
	 * @param radius Radius of the spherical add/remove brush in world units.
	 * @param adds_solid_terrain True adds terrain; false cuts terrain away.
	 * @return True when a valid planet component accepted and saved the edit stamp.
	 * A later planet update invalidates intersecting loaded chunks and requests
	 * their regeneration; this call does not synchronously rebuild geometry.
	 */

	/** @brief Applies a terrain stamp at a world-space position on the given planet body. */
	bool editPlanetGround(World& world, WorldBodyId body_id, const glm::dvec3& position,
		double radius, bool adds_solid_terrain);
}
