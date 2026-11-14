#include <cmath>

#include <danikk_space_engine/profiler.h>
#include <danikk_space_engine/component/point/local_player_controller.h>
#include <danikk_space_engine/component/world_body/planet/chunk_cache.h>
#include <danikk_space_engine/component/world_body/planet/planet.h>
#include <danikk_space_engine/world/face_ptr.h>
#include <danikk_space_engine/world/world.h>

namespace danikk_space_engine
{
	namespace
	{
		constexpr double chunk_update_distance = planet_voxel_size * 2.0;
		constexpr double terrain_edit_radius = 6.0;
		constexpr double invalid_chunk_diagonal = planet_voxel_size * 1.75;
		constexpr uint8 empty_bucket = 0;
		constexpr uint8 occupied_bucket = 1;
		constexpr uint8 deleted_bucket = 2;

		bool sameChunkKey(const glm::ivec3& left, const glm::ivec3& right)
		{
			return left.x == right.x && left.y == right.y && left.z == right.z;
		}

		uint64 hashChunkKey(const glm::ivec3& key)
		{
			uint64 hash = 1469598103934665603ull;
			for(int32 coordinate : {key.x, key.y, key.z})
			{
				hash ^= static_cast<uint32>(coordinate);
				hash *= 1099511628211ull;
			}
			hash ^= hash >> 30;
			hash *= 0xbf58476d1ce4e5b9ull;
			hash ^= hash >> 27;
			hash *= 0x94d049bb133111ebull;
			return hash ^ (hash >> 31);
		}

		size_t findChunkBucket(
			const DynamicArray<PlanetChunkLookupEntry>& entries,
			const glm::ivec3& key,
			bool find_deleted_bucket)
		{
			if(entries.size() == 0) return static_cast<size_t>(-1);
			const size_t mask = entries.size() - 1;
			size_t index = static_cast<size_t>(hashChunkKey(key)) & mask;
			size_t first_deleted = static_cast<size_t>(-1);
			for(size_t checked = 0; checked < entries.size(); checked++)
			{
				const PlanetChunkLookupEntry& entry = entries[index];
				if(entry.state == empty_bucket)
				{
					return find_deleted_bucket && first_deleted != static_cast<size_t>(-1)
						? first_deleted
						: index;
				}
				if(entry.state == occupied_bucket && sameChunkKey(entry.grid_coordinate, key)) return index;
				if(find_deleted_bucket && entry.state == deleted_bucket && first_deleted == static_cast<size_t>(-1))
				{
					first_deleted = index;
				}
				index = (index + 1) & mask;
			}
			return first_deleted;
		}

		void ensureChunkLookupCapacity(PlanetChunkLookup& lookup)
		{
			if(lookup.entries.size() != 0
				&& (lookup.used_bucket_count + 1) * 10 < lookup.entries.size() * 7) return;

			const size_t new_capacity = lookup.entries.size() == 0 ? 16 : lookup.entries.size() * 2;
			DynamicArray<PlanetChunkLookupEntry> new_entries;
			new_entries.resize(new_capacity);
			for(const PlanetChunkLookupEntry& old_entry : lookup.entries)
			{
				if(old_entry.state != occupied_bucket) continue;
				size_t index = static_cast<size_t>(hashChunkKey(old_entry.grid_coordinate)) & (new_capacity - 1);
				while(new_entries[index].state == occupied_bucket) index = (index + 1) & (new_capacity - 1);
				new_entries[index] = old_entry;
			}
			lookup.entries = static_cast<DynamicArray<PlanetChunkLookupEntry>&&>(new_entries);
			lookup.used_bucket_count = lookup.entry_count;
		}

		template<typename value_t>
		void eraseAt(DynamicArray<value_t>& values, size_t index)
		{
			for(size_t shift = index + 1; shift < values.size(); shift++)
			{
				values[shift - 1] = static_cast<value_t&&>(values[shift]);
			}
			values.resize(values.size() - 1);
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

		void unloadChunksOutsideDesiredSet(PlanetComponent& planet, World& world)
		{
			Watcher watcher("Planet::unloadChunks");
			for(size_t index = 0; index < planet.chunks.size();)
			{
				const PlanetChunkRecord& record = planet.chunks[index];
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
				eraseAt(planet.chunks, index);
			}
		}

		void clearFaceSelection(World& world)
		{
			Watcher watcher("Planet::clearFaceSelection");
			if(!world.data.isPointComponentSlotActive(world.local_player_controller_component_slot_id)) return;
			LocalPlayerControllerComponent& controller = world.getLocalPlayerControllerComponent();
			controller.selected_faces.clear();
			controller.selected_face_count = 0;
		}

		/*
		 * Builds the desired set of planet-local Cartesian chunks intersecting the camera's load sphere.
		 * Existing chunks are retained, missing keys receive immutable generation requests, and loaded
		 * chunks outside the new set are released. Terrain edits are copied into each request snapshot.
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
			planet.chunk_loading_active = true;
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
						if(planet.chunk_lookup.hasChunk(key)) continue;

						ChunkGenerationRequest request;
						request.world_body_id = slot.world_body_id;
						request.component_slot_id = slot.slot_id;
						request.component_generation_id = slot.generation_id;
						request.terrain_revision = planet.terrain_revision;
						request.planet_radius = planet.radius;
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
			unloadChunksOutsideDesiredSet(planet, world);
			clearFaceSelection(world);
		}

		void invalidateChunksAroundEdit(PlanetComponent& planet, World& world, const glm::dvec3& center, double radius)
		{
			Watcher watcher("Planet::invalidateChunks");
			for(size_t index = 0; index < planet.chunks.size();)
			{
				const PlanetChunkRecord& record = planet.chunks[index];
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
				eraseAt(planet.chunks, index);
			}
		}
	}

	void PlanetChunkLookup::clearDesired()
	{
		for(PlanetChunkLookupEntry& entry : entries)
		{
			if(entry.state != occupied_bucket) continue;
			entry.desired = false;
			if(entry.has_record) continue;
			entry.state = deleted_bucket;
			entry_count--;
		}
		if(entry_count == 0)
		{
			entries.clear();
			used_bucket_count = 0;
		}
	}

	void PlanetChunkLookup::markDesired(const glm::ivec3& grid_coordinate)
	{
		ensureChunkLookupCapacity(*this);
		const size_t index = findChunkBucket(entries, grid_coordinate, true);
		PlanetChunkLookupEntry& entry = entries[index];
		if(entry.state != occupied_bucket)
		{
			if(entry.state == empty_bucket) used_bucket_count++;
			entry.grid_coordinate = grid_coordinate;
			entry.chunk_id = static_cast<ChunkId>(-1);
			entry.has_record = false;
			entry.desired = false;
			entry.state = occupied_bucket;
			entry_count++;
		}
		entry.desired = true;
	}

	void PlanetChunkLookup::setChunk(const glm::ivec3& grid_coordinate, ChunkId chunk_id)
	{
		ensureChunkLookupCapacity(*this);
		const size_t index = findChunkBucket(entries, grid_coordinate, true);
		PlanetChunkLookupEntry& entry = entries[index];
		if(entry.state != occupied_bucket)
		{
			if(entry.state == empty_bucket) used_bucket_count++;
			entry.grid_coordinate = grid_coordinate;
			entry.desired = false;
			entry.state = occupied_bucket;
			entry_count++;
		}
		entry.chunk_id = chunk_id;
		entry.has_record = true;
	}

	void PlanetChunkLookup::remove(const glm::ivec3& grid_coordinate)
	{
		const size_t index = findChunkBucket(entries, grid_coordinate, false);
		if(index == static_cast<size_t>(-1)) return;
		PlanetChunkLookupEntry& entry = entries[index];
		entry.has_record = false;
		entry.chunk_id = static_cast<ChunkId>(-1);
		if(entry.desired) return;
		entry.state = deleted_bucket;
		entry_count--;
	}

	bool PlanetChunkLookup::isDesired(const glm::ivec3& grid_coordinate) const
	{
		const size_t index = findChunkBucket(entries, grid_coordinate, false);
		return index != static_cast<size_t>(-1) && entries[index].desired;
	}

	bool PlanetChunkLookup::hasChunk(const glm::ivec3& grid_coordinate) const
	{
		const size_t index = findChunkBucket(entries, grid_coordinate, false);
		return index != static_cast<size_t>(-1) && entries[index].has_record;
	}

	void constructPlanetComponent(void* destination, const void* init_data)
	{
		Watcher watcher("constructPlanetComponent");
		if(destination == nullptr) danikk_engine::formatFatalError("Planet component construction received null destination");
		new (destination) PlanetComponent();
		if(init_data != nullptr)
		{
			const PlanetComponentSettings& settings = *static_cast<const PlanetComponentSettings*>(init_data);
			PlanetComponent& planet = *static_cast<PlanetComponent*>(destination);
			planet.radius = settings.radius;
			planet.chunk_load_radius = settings.chunk_load_radius;
			planet.noise_seed = settings.noise_seed;
		}
	}

	void movePlanetComponent(void* destination, void* source)
	{
		Watcher watcher("movePlanetComponent");
		if(destination == nullptr || source == nullptr)
		{
			danikk_engine::formatFatalError("Planet component move received null memory");
		}
		PlanetComponent* source_planet = static_cast<PlanetComponent*>(source);
		new (destination) PlanetComponent(static_cast<PlanetComponent&&>(*source_planet));
		source_planet->~PlanetComponent();
	}

	void destroyPlanetComponent(void* data)
	{
		Watcher watcher("destroyPlanetComponent");
		if(data == nullptr) danikk_engine::formatFatalError("Planet component destruction received null data");
		static_cast<PlanetComponent*>(data)->~PlanetComponent();
	}

	void framePlanetComponent(WorldBodyComponentSlot& slot, World& world)
	{
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
			if(planet.chunk_loading_active)
			{
				planet.chunk_lookup.clearDesired();
				planet.pending_requests.clear();
				unloadChunksOutsideDesiredSet(planet, world);
				queuePlanetRequests(planet, world, slot);
				planet.chunk_loading_active = false;
				planet.chunk_set_initialized = false;
				clearFaceSelection(world);
			}
			return;
		}

		while(planet.processed_edit_count < planet.terrain_edits.size())
		{
			const PlanetTerrainEdit& edit = planet.terrain_edits[planet.processed_edit_count++];
			invalidateChunksAroundEdit(planet, world, edit.center, edit.radius);
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

	void acceptPlanetGeneratedChunk(
		WorldBodyComponentSlot& slot,
		World& world,
		const GeneratedChunkData& generated_chunk)
	{
		Watcher watcher("Accept generated planet chunk");
		// Reject stale asynchronous results. Empty chunks are recorded without a WorldData allocation so
		// the loader does not repeatedly generate known-empty space; non-empty results become ground triangles.
		PlanetComponent& planet = *static_cast<PlanetComponent*>(slot.getData());
		if(generated_chunk.terrain_revision != planet.terrain_revision
			|| !planet.chunk_lookup.isDesired(generated_chunk.grid_coordinate)
			|| planet.chunk_lookup.hasChunk(generated_chunk.grid_coordinate)) return;

		ChunkId chunk_id = static_cast<ChunkId>(-1);
		if(generated_chunk.triangle_point_indices.size() > 0)
		{
			Chunk& chunk = world.data.createChunk(slot.world_body_id);
			chunk_id = chunk.chunk_id;
			chunk.grid_coordinate = generated_chunk.grid_coordinate;
			chunk.position = generated_chunk.chunk_position;
			DynamicArray<PointId> point_ids;
			point_ids.reserve(generated_chunk.point_positions.size());
			for(const vec3& position : generated_chunk.point_positions)
			{
				const PointId point_id = world.data.createPoint();
				world.data.getPoint(point_id).position = position;
				point_ids.push(point_id);
			}
			for(PointId point_id : point_ids) world.data.addPointReferenceToChunk(chunk_id, point_id);
			for(size_t index = 0; index + 2 < generated_chunk.triangle_point_indices.size(); index += 3)
			{
				const uint32 point_a_index = generated_chunk.triangle_point_indices[index];
				const uint32 point_b_index = generated_chunk.triangle_point_indices[index + 1];
				const uint32 point_c_index = generated_chunk.triangle_point_indices[index + 2];
				if(point_a_index >= point_ids.size() || point_b_index >= point_ids.size() || point_c_index >= point_ids.size())
				{
					danikk_engine::formatFatalError("Generated planet chunk has an invalid point index");
				}
				const GroundTriangleId triangle_id = world.data.createGroundTriangle();
				GroundTriangle& triangle = world.data.getGroundTriangle(triangle_id);
				triangle.points[0] = point_ids[point_a_index];
				triangle.points[1] = point_ids[point_b_index];
				triangle.points[2] = point_ids[point_c_index];
				world.data.addGroundTriangleToChunk(chunk_id, triangle_id);
			}
		}
		planet.chunks.pushMove(PlanetChunkRecord{generated_chunk.grid_coordinate, chunk_id});
		planet.chunk_lookup.setChunk(generated_chunk.grid_coordinate, chunk_id);
		world.removeChunkGenerationRequest(slot.slot_id, generated_chunk.grid_coordinate);
		for(size_t index = 0; index < planet.pending_requests.size(); index++)
		{
			if(!sameChunkKey(planet.pending_requests[index].grid_coordinate, generated_chunk.grid_coordinate)) continue;
			eraseAt(planet.pending_requests, index);
			break;
		}
	}

	bool editPlanetGround(World& world, const FacePtr& face, double radius, bool add_material)
	{
		Watcher watcher("editPlanetGround");
		if(!face.is_ground_triangle || !world.data.isChunkActive(face.chunk_id)
			|| !world.data.isGroundTriangleActive(face.ground_triangle_id)) return false;
		Chunk& chunk = world.data.getChunk(face.chunk_id);
		if(!world.data.isWorldBodyActive(chunk.world_body_id)) return false;
		WorldBody& body = world.data.getWorldBody(chunk.world_body_id);
		const GroundTriangle& triangle = world.data.getGroundTriangle(face.ground_triangle_id);
		glm::dvec3 center = chunk.position;
		for(PointId point_id : triangle.points)
		{
			if(!world.data.isPointActive(point_id)) return false;
			center += glm::dvec3(world.data.getPoint(point_id).position) / 3.0;
		}

		for(WorldBodyComponentSlotId slot_id : body.component_slot_ids)
		{
			if(!world.data.isWorldBodyComponentSlotActive(slot_id)) continue;
			WorldBodyComponentSlot& slot = world.data.getWorldBodyComponentSlot(slot_id);
			if(slot.type_id != WorldBodyComponentTypeId::planet) continue;
			PlanetComponent& planet = *static_cast<PlanetComponent*>(slot.getData());
			planet.terrain_edits.pushMove(PlanetTerrainEdit{center, radius, add_material});
			planet.terrain_revision++;
			if(planet.terrain_revision == 0) planet.terrain_revision++;
			savePlanetTerrainEdits(
				slot.world_body_id,
				planet.radius,
				planet.noise_seed,
				planet.terrain_edits,
				planet.terrain_revision);
			return true;
		}
		return false;
	}
}
