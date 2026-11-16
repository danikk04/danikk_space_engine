#include <cmath>

#include <danikk_space_engine/profiler.h>
#include <danikk_space_engine/component/point/local_player_controller.h>
#include <danikk_space_engine/component/world_body/planet/planet.h>
#include <danikk_space_engine/world/world.h>

namespace danikk_space_engine
{
	namespace
	{
		constexpr double chunk_update_distance = planet_voxel_size * 2.0;

		template<typename value_t>
		void eraseAt(DynamicArray<value_t>& values, size_t index)
		{
			for(size_t shift = index + 1; shift < values.size(); shift++)
			{
				values[shift - 1] = static_cast<value_t&&>(values[shift]);
			}
			values.resize(values.size() - 1);
		}
	}

	void queuePlanetRequests(PlanetComponent& planet, World& world, const WorldBodyComponentSlot& slot)
	{
		Watcher watcher("Planet::queueRequests");
		for(ChunkGenerationRequest& request : planet.pending_requests)
		{
			request.world_body_id = slot.world_body_id;
			request.component_slot_id = slot.slot_id;
			request.component_generation_id = slot.generation_id;
		}
		world.replaceChunkGenerationRequests(slot.slot_id, planet.pending_requests);
	}

	void unloadPlanetChunksOutsideDesiredSet(PlanetComponent& planet, World& world)
	{
		Watcher watcher("Planet::unloadChunks");
		for(size_t index = 0; index < planet.chunk_records.size();)
		{
			const PlanetChunkRecord& record = planet.chunk_records[index];
			if(planet.chunk_lookup.isDesired(record.grid_coordinate))
			{
				index++;
				continue;
			}
			if(record.chunk_id != static_cast<ChunkId>(-1) && world.data.isChunkActive(record.chunk_id))
			{
				world.data.destroyChunk(record.chunk_id);
			}
			planet.chunk_lookup.remove(record.grid_coordinate);
			eraseAt(planet.chunk_records, index);
		}
	}

	void clearPlanetFaceSelection(World& world)
	{
		Watcher watcher("Planet::clearFaceSelection");
		if(!world.data.isPointComponentSlotActive(world.local_player_controller_component_slot_id)) return;
		LocalPlayerControllerComponent& controller = world.getLocalPlayerControllerComponent();
		controller.selected_faces.clear();
		controller.selected_face_count = 0;
	}

	/*
	 * Rebuilds the desired Cartesian chunk set around the camera, creates generation requests for
	 * missing entries, and unloads records outside the new set. Requests capture terrain edits so the
	 * worker sees a stable snapshot while generation runs asynchronously.
	 */
	void updatePlanetChunks(
		PlanetComponent& planet,
		WorldBodyComponentSlot& slot,
		World& world,
		const glm::dvec3& camera_position)
	{
		Watcher watcher("Planet chunk set update");
		planet.last_chunk_update_position = camera_position;
		planet.last_chunk_update_revision = planet.terrain_revision;
		planet.chunk_set_initialized = true;
		planet.chunk_updates_active = true;
		const double chunk_width = planet_chunk_width;
		const glm::dvec3 range(planet.chunk_load_radius);
		const glm::ivec3 minimum_key(
			static_cast<int>(std::floor((camera_position.x - range.x) / chunk_width)),
			static_cast<int>(std::floor((camera_position.y - range.y) / chunk_width)),
			static_cast<int>(std::floor((camera_position.z - range.z) / chunk_width)));
		const glm::ivec3 maximum_key(
			static_cast<int>(std::floor((camera_position.x + range.x) / chunk_width)),
			static_cast<int>(std::floor((camera_position.y + range.y) / chunk_width)),
			static_cast<int>(std::floor((camera_position.z + range.z) / chunk_width)));
		const double radius_squared = planet.chunk_load_radius * planet.chunk_load_radius;
		DynamicArray<ChunkGenerationRequest> requests;
		planet.chunk_lookup.clearDesired();

		for(int z = minimum_key.z; z <= maximum_key.z; z++)
		{
			for(int y = minimum_key.y; y <= maximum_key.y; y++)
			{
				for(int x = minimum_key.x; x <= maximum_key.x; x++)
				{
					const glm::ivec3 key(x, y, z);
					const glm::dvec3 chunk_minimum = glm::dvec3(key) * chunk_width;
					const glm::dvec3 chunk_maximum = chunk_minimum + glm::dvec3(chunk_width);
					const glm::dvec3 closest = glm::clamp(camera_position, chunk_minimum, chunk_maximum);
					if(glm::dot(camera_position - closest, camera_position - closest) > radius_squared) continue;

					planet.chunk_lookup.markDesired(key);
					if(planet.chunk_lookup.isGenerated(key)) continue;

					ChunkGenerationRequest request;
					request.world_body_id = slot.world_body_id;
					request.component_slot_id = slot.slot_id;
					request.component_generation_id = slot.generation_id;
					request.terrain_revision = planet.terrain_revision;
					request.planet_radius = planet.radius;
				request.core_radius = planet.core_radius;
					request.chunk_width = chunk_width;
					request.voxel_size = planet_voxel_size;
					request.noise_seed = planet.noise_seed;
					request.grid_coordinate = key;
					request.chunk_position = chunk_minimum;
					request.terrain_edits = planet.terrain_edits;
					requests.push(request);
				}
			}
		}

		planet.pending_requests = static_cast<DynamicArray<ChunkGenerationRequest>&&>(requests);
		unloadPlanetChunksOutsideDesiredSet(planet, world);
		clearPlanetFaceSelection(world);
	}

	void invalidatePlanetChunksAroundEdit(
		PlanetComponent& planet,
		World& world,
		const glm::dvec3& center,
		double radius)
	{
		Watcher watcher("Planet::invalidateChunks");
		for(size_t index = 0; index < planet.chunk_records.size();)
		{
			const PlanetChunkRecord& record = planet.chunk_records[index];
			const glm::dvec3 minimum = glm::dvec3(record.grid_coordinate) * planet_chunk_width;
			const glm::dvec3 maximum = minimum + glm::dvec3(planet_chunk_width);
			const glm::dvec3 closest = glm::clamp(center, minimum, maximum);
			const double invalidation_radius = radius + planet_voxel_size * 2.0;
			if(glm::dot(center - closest, center - closest) > invalidation_radius * invalidation_radius)
			{
				index++;
				continue;
			}
			if(record.chunk_id != static_cast<ChunkId>(-1) && world.data.isChunkActive(record.chunk_id))
			{
				world.data.destroyChunk(record.chunk_id);
			}
			planet.chunk_lookup.remove(record.grid_coordinate);
			eraseAt(planet.chunk_records, index);
		}
	}
}
