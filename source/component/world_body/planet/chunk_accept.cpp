#include <danikk_space_engine/profiler.h>
#include <danikk_space_engine/component/world_body/planet/planet.h>
#include <danikk_space_engine/world/world.h>

namespace danikk_space_engine
{
	namespace
	{
		bool sameChunkKey(const glm::ivec3& left, const glm::ivec3& right)
		{
			return left.x == right.x && left.y == right.y && left.z == right.z;
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
	}

	void acceptPlanetGeneratedChunk(
		WorldBodyComponentSlot& slot,
		World& world,
		const GeneratedChunkData& generated_chunk)
	{
		Watcher watcher("Accept generated planet chunk");
		// Discard stale results. Empty chunks are recorded without allocating WorldData resources;
		// non-empty results are converted from local mesh indices into points and ground triangles.
		PlanetComponent& planet = *static_cast<PlanetComponent*>(slot.getData());
		if(generated_chunk.terrain_revision != planet.terrain_revision
			|| !planet.chunk_lookup.isDesired(generated_chunk.grid_coordinate)
			|| planet.chunk_lookup.isGenerated(generated_chunk.grid_coordinate)) return;

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
		planet.chunk_records.pushMove(PlanetChunkRecord{generated_chunk.grid_coordinate, chunk_id});
		planet.chunk_lookup.setChunk(generated_chunk.grid_coordinate, chunk_id);
		world.removeChunkGenerationRequest(slot.slot_id, generated_chunk.grid_coordinate);
		for(size_t index = 0; index < planet.pending_requests.size(); index++)
		{
			if(!sameChunkKey(planet.pending_requests[index].grid_coordinate, generated_chunk.grid_coordinate)) continue;
			eraseAt(planet.pending_requests, index);
			break;
		}
	}
}
