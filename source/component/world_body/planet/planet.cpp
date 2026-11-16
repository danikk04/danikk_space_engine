#include <danikk_space_engine/profiler.h>
#include <danikk_space_engine/component/world_body/planet/chunk_save.h>
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
	 * @brief Returns planet settings.
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
	 * @brief Constructs planet component.
	 */
	void constructPlanetComponent(void* destination, const void* init_data)
	{
		/**
		 * @brief Disables copying to ensure a measured scope is recorded once.
		 */
		Watcher watcher("constructPlanetComponent");
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
	 * @brief Moves planet component.
	 */
	void movePlanetComponent(void* destination, void* source)
	{
		/**
		 * @brief Disables copying to ensure a measured scope is recorded once.
		 */
		Watcher watcher("movePlanetComponent");
		if(destination == nullptr || source == nullptr)
		{
			danikk_engine::formatFatalError("Planet component move received null memory");
		}
		PlanetComponent* source_planet = static_cast<PlanetComponent*>(source);
		new (destination) PlanetComponent(static_cast<PlanetComponent&&>(*source_planet));
		source_planet->~PlanetComponent();
	}

	/**
	 * @brief Removes and releases planet component.
	 */
	void destroyPlanetComponent(void* data)
	{
		/**
		 * @brief Disables copying to ensure a measured scope is recorded once.
		 */
		Watcher watcher("destroyPlanetComponent");
		if(data == nullptr) danikk_engine::formatFatalError("Planet component destruction received null data");
		static_cast<PlanetComponent*>(data)->~PlanetComponent();
	}

	/**
	 * @brief Updates planet component.
	 */
	void updatePlanetComponent(WorldBodyComponentSlot& slot, World& world)
	{
		/**
		 * @brief Disables copying to ensure a measured scope is recorded once.
		 */
		Watcher watcher("Planet component frame");
		// Keep 3D chunks around the camera while it is within reach of the planet. Update the set after
		// movement or terrain edits; the field itself remains defined throughout the planet volume.
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
				clearPlanetFaceSelection(world);
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
			|| planet.last_chunk_update_revision != planet.terrain_revision;
		if(update_chunks)
		{
			updatePlanetChunks(planet, slot, world, camera_position);
			queuePlanetRequests(planet, world, slot);
		}
	}
}
