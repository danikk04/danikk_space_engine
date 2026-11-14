#include <cmath>

#include <danikk_space_engine/component/point/local_player_controller.h>
#include <danikk_space_engine/component/world_body/planet/planet.h>
#include <danikk_space_engine/component/world_body/planet/terrain.h>
#include <danikk_space_engine/world/world.h>

namespace danikk_space_engine
{
	namespace
	{
		constexpr int planet_chunks_per_face = 32;
		constexpr double chunk_update_distance = 8.0;

		struct PlanetCubeFace
		{
			glm::dvec3 normal;
			glm::dvec3 right;
			glm::dvec3 up;
		};

		PlanetCubeFace getPlanetCubeFace(int face_index)
		{
			switch(face_index)
			{
				case 0: return {glm::dvec3( 1, 0, 0), glm::dvec3( 0, 1, 0), glm::dvec3(0, 0, 1)};
				case 1: return {glm::dvec3(-1, 0, 0), glm::dvec3( 0,-1, 0), glm::dvec3(0, 0, 1)};
				case 2: return {glm::dvec3( 0, 1, 0), glm::dvec3(-1, 0, 0), glm::dvec3(0, 0, 1)};
				case 3: return {glm::dvec3( 0,-1, 0), glm::dvec3( 1, 0, 0), glm::dvec3(0, 0, 1)};
				case 4: return {glm::dvec3( 0, 0, 1), glm::dvec3( 1, 0, 0), glm::dvec3(0, 1, 0)};
				default:return {glm::dvec3( 0, 0,-1), glm::dvec3(-1, 0, 0), glm::dvec3(0, 1, 0)};
			}
		}

		bool sameChunkKey(const glm::ivec3& left, const glm::ivec3& right)
		{
			return left.x == right.x && left.y == right.y && left.z == right.z;
		}

		bool hasChunk(const DynamicArray<PlanetChunkRecord>& chunks, const glm::ivec3& key)
		{
			for(const PlanetChunkRecord& chunk : chunks)
			{
				if(sameChunkKey(chunk.grid_coordinate, key)) return true;
			}
			return false;
		}

		bool hasKey(const DynamicArray<glm::ivec3>& keys, const glm::ivec3& key)
		{
			for(const glm::ivec3& existing : keys)
			{
				if(sameChunkKey(existing, key)) return true;
			}
			return false;
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
			for(ChunkGenerationRequest& request : planet.pending_requests)
			{
				request.world_body_id = slot.world_body_id;
				request.component_slot_id = slot.slot_id;
				request.component_generation_id = slot.generation_id;
				world.queueChunkGenerationRequest(request);
			}
		}

		void unloadChunksOutsideDesiredSet(PlanetComponent& planet, World& world)
		{
			for(size_t index = 0; index < planet.chunks.size();)
			{
				PlanetChunkRecord& record = planet.chunks[index];
				if(hasKey(planet.desired_chunk_keys, record.grid_coordinate))
				{
					index++;
					continue;
				}
				if(world.data.isChunkActive(record.chunk_id)) world.data.destroyChunk(record.chunk_id);
				eraseAt(planet.chunks, index);
			}
		}

		void clearFaceSelection(World& world)
		{
			if(!world.data.isPointComponentSlotActive(world.local_player_controller_component_slot_id)) return;
			LocalPlayerControllerComponent& controller = world.getLocalPlayerControllerComponent();
			controller.selected_faces.clear();
			controller.selected_face_count = 0;
		}

		void updatePlanetChunks(
			PlanetComponent& planet,
			WorldBodyComponentSlot& slot,
			World& world,
			const glm::dvec3& camera_direction)
		{
			// Rebuild the desired cube-sphere tile set around the camera direction, retain already loaded tiles,
			// queue only missing tiles, and unload tiles outside the new set. Each request carries a snapshot
			// of the planet generation parameters so the worker does not access component or world state.
			planet.last_chunk_update_direction = camera_direction;
			planet.chunk_set_initialized = true;
			planet.chunk_loading_active = true;
			const double chunk_width = 2.0 * planet.radius / planet_chunks_per_face;
			const double tile_radius = chunk_width * 0.72;
			const double max_center_distance = planet.chunk_load_radius + tile_radius;
			const double min_center_dot = std::cos(max_center_distance / planet.radius);
			DynamicArray<glm::ivec3> desired_keys;
			DynamicArray<ChunkGenerationRequest> requests;

			for(int face_index = 0; face_index < 6; face_index++)
			{
				const PlanetCubeFace face = getPlanetCubeFace(face_index);
				for(int chunk_y = 0; chunk_y < planet_chunks_per_face; chunk_y++)
				{
					for(int chunk_x = 0; chunk_x < planet_chunks_per_face; chunk_x++)
					{
						const double x0 = -planet.radius + chunk_x * chunk_width;
						const double y0 = -planet.radius + chunk_y * chunk_width;
						const double center_x = x0 + chunk_width * 0.5;
						const double center_y = y0 + chunk_width * 0.5;
						const glm::dvec3 center_direction = glm::normalize(
							face.normal * planet.radius
								+ face.right * center_x
								+ face.up * center_y);
						if(glm::dot(center_direction, camera_direction) < min_center_dot) continue;

						const glm::ivec3 key(chunk_x, chunk_y, face_index);
						desired_keys.push(key);
						if(hasChunk(planet.chunks, key)) continue;

						const glm::dvec3 direction = glm::normalize(
							face.normal * planet.radius
								+ face.right * center_x
								+ face.up * center_y);
						const glm::dvec3 surface_sample = direction * planet.radius;
						const double height = samplePlanetScalarField(surface_sample, planet.radius, planet.noise_seed);
						ChunkGenerationRequest request;
						request.world_body_id = slot.world_body_id;
						request.component_slot_id = slot.slot_id;
						request.component_generation_id = slot.generation_id;
						request.planet_radius = planet.radius;
						request.noise_seed = planet.noise_seed;
						request.grid_coordinate = key;
						request.chunk_position = direction * (planet.radius + height);
						request.up = face.normal;
						request.east = face.right;
						request.north = face.up;
						request.x0 = x0;
						request.y0 = y0;
						request.width = chunk_width;
						requests.push(request);
					}
				}
			}

			planet.desired_chunk_keys = static_cast<DynamicArray<glm::ivec3>&&>(desired_keys);
			planet.pending_requests = static_cast<DynamicArray<ChunkGenerationRequest>&&>(requests);
			unloadChunksOutsideDesiredSet(planet, world);
			clearFaceSelection(world);
		}
	}

	void constructPlanetComponent(void* destination, const void* init_data)
	{
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
		if(data == nullptr) danikk_engine::formatFatalError("Planet component destruction received null data");
		static_cast<PlanetComponent*>(data)->~PlanetComponent();
	}

	void framePlanetComponent(WorldBodyComponentSlot& slot, World& world)
	{
		// Track the camera relative to this body. Outside the chunk loading range, discard loaded tiles;
		// inside it, refresh the desired tile set only after the camera moves far enough, then re-submit its
		// pending requests to the loader, which deduplicates requests already queued or being generated.
		if(!world.data.isWorldBodyActive(slot.world_body_id)
			|| !world.data.isPointComponentSlotActive(world.camera_component_slot_id)) return;

		PlanetComponent& planet = *static_cast<PlanetComponent*>(slot.getData());
		const glm::dvec3 body_position = world.data.getWorldBody(slot.world_body_id).position;
		const glm::dvec3 radial = world.getCameraWorldPosition() - body_position;
		const double radial_length = glm::length(radial);
		if(radial_length <= 0.000001) return;
		const double chunk_width = 2.0 * planet.radius / planet_chunks_per_face;
		const double tile_radius = chunk_width * 0.72;
		const bool in_chunk_range = std::abs(radial_length - planet.radius)
			<= planet.chunk_load_radius + tile_radius;
		if(!in_chunk_range)
		{
			if(planet.chunk_loading_active)
			{
				planet.desired_chunk_keys.clear();
				planet.pending_requests.clear();
				unloadChunksOutsideDesiredSet(planet, world);
				planet.chunk_loading_active = false;
				planet.chunk_set_initialized = false;
				clearFaceSelection(world);
			}
			return;
		}

		const glm::dvec3 camera_direction = radial / radial_length;
		bool update_chunks = !planet.chunk_set_initialized;
		if(planet.chunk_set_initialized)
		{
			const double direction_dot = glm::clamp(glm::dot(camera_direction, planet.last_chunk_update_direction), -1.0, 1.0);
			const double surface_distance = std::acos(direction_dot) * planet.radius;
			update_chunks = surface_distance > chunk_update_distance;
		}
		if(update_chunks) updatePlanetChunks(planet, slot, world, camera_direction);
		queuePlanetRequests(planet, world, slot);
	}

	void acceptPlanetGeneratedChunk(
		WorldBodyComponentSlot& slot,
		World& world,
		const GeneratedChunkData& generated_chunk)
	{
		// Reject stale or no-longer-needed results, then materialize the generated local point positions and
		// triangle indices as WorldData resources owned by a new chunk. Finally record the chunk and remove
		// its request from the component's pending list so subsequent frames do not resubmit it.
		PlanetComponent& planet = *static_cast<PlanetComponent*>(slot.getData());
		if(!hasKey(planet.desired_chunk_keys, generated_chunk.grid_coordinate)
			|| hasChunk(planet.chunks, generated_chunk.grid_coordinate)) return;

		Chunk& chunk = world.data.createChunk(slot.world_body_id);
		const ChunkId chunk_id = chunk.chunk_id;
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
		planet.chunks.pushMove(PlanetChunkRecord{generated_chunk.grid_coordinate, chunk_id});
		for(size_t index = 0; index < planet.pending_requests.size(); index++)
		{
			if(!sameChunkKey(planet.pending_requests[index].grid_coordinate, generated_chunk.grid_coordinate)) continue;
			eraseAt(planet.pending_requests, index);
			break;
		}
	}
}
