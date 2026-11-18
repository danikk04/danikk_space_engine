#include <danikk_space_engine/profiler.h>
#include <danikk_space_engine/config.h>
#include <danikk_space_engine/component/world_body/planet/terrain_edit_save.h>
#include <danikk_space_engine/component/world_body/planet/planet.h>
#include <danikk_space_engine/world/world.h>

#include "planet_internal.h"

namespace danikk_space_engine
{
	namespace
	{
		constexpr double chunk_update_distance = planet_voxel_size * 2.0;
		const PlanetComponentSettings planet_settings[planet_settings_ids::count] =
		{
			{"Earth", default_planet_radius, default_planet_core_radius, planet_chunk_load_radius, 9.8, 0}
		};
	}

	/**
	 * @brief Returns the immutable preset used to initialize a planet component.
	 * @param id Index into the static planet preset table.
	 * @return The requested preset; an out-of-range index terminates with a fatal error.
	 */
	const PlanetComponentSettings& getPlanetSettings(PlanetSettingsId id)
	{
		if(id >= planet_settings_ids::count)
		{
			danikk_engine::formatFatalError("Invalid planet settings ID %", id);
		}
		return planet_settings[id];
	}

	/**
	 * @brief Constructs a default planet payload and optionally copies a preset's scalar settings.
	 *
	 * The preset supplies name, radii, load distance, surface gravity, and noise seed. Runtime
	 * arrays and chunk-tracking state remain default-initialized for this new component instance.
	 * @param destination Component payload storage provided by WorldData.
	 * @param init_data Optional PlanetComponentSettings preset; null selects Earth defaults.
	 */
	void constructPlanetComponent(void* destination, const void* init_data)
	{
		/**
		 * @brief Measures the duration of the enclosing profiler scope.
		 */
		DSE_WATCH();
		if(destination == nullptr) danikk_engine::formatFatalError("Planet component construction received null destination");
		new (destination) PlanetComponent();
		if(init_data != nullptr)
		{
			const PlanetComponentSettings& settings = *static_cast<const PlanetComponentSettings*>(init_data);
			PlanetComponent& planet = *static_cast<PlanetComponent*>(destination);
			planet.name = settings.name;
			planet.radius = settings.radius;
			planet.core_radius = settings.core_radius;
			planet.chunk_load_radius = settings.chunk_load_radius;
			planet.surface_gravity = settings.surface_gravity;
			planet.noise_seed = settings.noise_seed;
		}
	}

	/**
	 * @brief Transfers owned edit, lookup, and request arrays into replacement component storage.
	 * @param destination Replacement payload storage.
	 * @param source Existing planet payload, destroyed after its arrays are moved.
	 */
	void movePlanetComponent(void* destination, void* source)
	{
		/**
		 * @brief Measures the duration of the enclosing profiler scope.
		 */
		DSE_WATCH();
		if(destination == nullptr || source == nullptr)
		{
			danikk_engine::formatFatalError("Planet component move received null memory");
		}
		PlanetComponent* source_planet = static_cast<PlanetComponent*>(source);
		new (destination) PlanetComponent(static_cast<PlanetComponent&&>(*source_planet));
		source_planet->~PlanetComponent();
	}

	/**
	 * @brief Ends the planet payload lifetime and releases its owned DynamicArray allocations.
	 * @param data Existing live planet payload.
	 */
	void destroyPlanetComponent(void* data)
	{
		/**
		 * @brief Measures the duration of the enclosing profiler scope.
		 */
		DSE_WATCH();
		if(data == nullptr) danikk_engine::formatFatalError("Planet component destruction received null data");
		static_cast<PlanetComponent*>(data)->~PlanetComponent();
	}

	/**
	 * @brief Loads persistent terrain edits and updates the desired chunk set around the camera.
	 *
	 * Chunk reconciliation runs only when the camera moves beyond the update threshold, terrain
	 * revision changes, or the set has not been initialized. Outside the planet's load range it
	 * clears pending work and unloads chunks; each update submits immutable requests to ChunkMeshGenerator.
	 * @param slot Planet component slot identifying the body and component generation.
	 * @param world World containing camera state, chunk resources, and the generation worker.
	 */
	void updatePlanetComponent(WorldBodyComponentSlot& slot, World& world)
	{
		/**
		 * @brief Measures the duration of the enclosing profiler scope.
		 */
		DSE_WATCH();
		/**
		 * @brief Keeps a camera-centered set of 3D chunks loaded and refreshes it after movement or edits.
		 * The scalar field remains defined throughout the planet volume; this set controls loaded surface work.
		 */
		if(!world.data.isWorldBodyActive(slot.world_body_id)
			|| !world.data.isPointComponentSlotActive(world.camera_component_slot_id)) return;

		PlanetComponent& planet = *static_cast<PlanetComponent*>(slot.getData());
		if(!planet.terrain_edits_loaded)
		{
			loadPlanetTerrainEdits(
				slot.world_body_id,
				planet.radius,
				planet.noise_seed,
				planet.terrain_edits,
				planet.terrain_revision);
			planet.terrain_edits_loaded = true;
			planet.processed_edit_count = planet.terrain_edits.size();
		}
		const glm::dvec3 body_position = world.data.getWorldBody(slot.world_body_id).position;
		const glm::dvec3 camera_position = world.getCameraWorldPosition() - body_position;
		const double distance_from_center = glm::length(camera_position);
		const bool in_chunk_range = distance_from_center
			<= planet.radius + planet.chunk_load_radius + planet_chunk_width;
		if(!in_chunk_range)
		{
			if(planet.chunk_updates_active)
			{
				planet.chunk_lookup.clearDesired();
				planet.pending_requests.clear();
				unloadPlanetChunksOutsideDesiredSet(planet, world);
				queuePlanetRequests(planet, world, slot);
				planet.chunk_updates_active = false;
				planet.chunk_set_initialized = false;
			}
			return;
		}

		while(planet.processed_edit_count < planet.terrain_edits.size())
		{
			const PlanetTerrainEdit& edit = planet.terrain_edits[planet.processed_edit_count++];
			invalidatePlanetChunksAroundEdit(planet, world, edit.center, edit.radius);
		}

		const double moved_distance = glm::length(camera_position - planet.last_chunk_update_position);
		const bool update_chunks = !planet.chunk_set_initialized
			|| moved_distance > chunk_update_distance
			|| planet.last_chunk_update_revision != planet.terrain_revision
			|| planet.last_chunk_update_mesh_generator != settings.planet_mesh_generator;
		if(update_chunks)
		{
			updatePlanetChunks(planet, slot, world, camera_position);
			queuePlanetRequests(planet, world, slot);
		}
	}
}
