#include <danikk_space_engine/profiler.h>
#include <danikk_space_engine/component/world_body/planet/planet.h>
#include <danikk_space_engine/component/world_body/planet/terrain.h>
#include <danikk_space_engine/world/world.h>

namespace danikk_space_engine
{
	namespace
	{

		/**
		 * @brief Tests exact equality of two integer planet-grid coordinates.
		 * @param left First grid key.
		 * @param right Second grid key.
		 * @return True when all three axes match.
		 */
		bool sameChunkKey(const glm::ivec3& left, const glm::ivec3& right)
		{
			return left.x == right.x && left.y == right.y && left.z == right.z;
		}

		/**
		 * @brief Removes one element while preserving order by moving later elements left.
		 * @tparam value_t Element type, required to support move assignment and destruction.
		 * @param values Array to modify.
		 * @param index Index of the element to remove; caller must provide an in-range index.
		 */
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

	/**
	 * @brief Rejects stale worker output or installs its points and ground triangles in WorldData.
	 *
	 * Non-empty results create a chunk, convert local vertex indices to pooled Point IDs, and
	 * create one material-tagged GroundTriangle per index triple. Empty results still receive a
	 * generated lookup record so the same empty coordinate is not queued repeatedly.
	 * @param slot Planet component slot whose identity was captured by the request.
	 * @param world World receiving accepted chunk resources.
	 * @param generated_chunk Worker output with request identity, local vertices, indices, and materials.
	 */
	void acceptPlanetGeneratedChunk(
		WorldBodyComponentSlot& slot,
		World& world,
		const GeneratedChunkData& generated_chunk)
	{
		/**
		 * @brief Measures the duration of the enclosing profiler scope.
		 */
		Watcher watcher("Accept generated planet chunk");
		/**
		 * @brief Rejects stale results and records valid empty chunks without allocating geometry.
		 * Non-empty results convert local mesh indices into WorldData points and ground triangles.
		 */
		PlanetComponent& planet = *static_cast<PlanetComponent*>(slot.getData());
		if(generated_chunk.terrain_revision != planet.terrain_revision
			|| !planet.chunk_lookup.isDesired(generated_chunk.grid_coordinate)
			|| planet.chunk_lookup.isGenerated(generated_chunk.grid_coordinate)) return;

		ChunkId chunk_id = static_cast<ChunkId>(-1);
		planet.chunk_lookup.getChunk(generated_chunk.grid_coordinate, chunk_id);
		if(world.data.isChunkActive(chunk_id))
		{
			Chunk& existing = world.data.getChunk(chunk_id);
			DynamicArray<PointId> old_points;
			for(GroundTriangleId triangle_id : existing.ground_triangle_ids)
			{
				const GroundTriangle& triangle = world.data.getGroundTriangle(triangle_id);
				for(PointId id : triangle.points) old_points.push(id);
			}
			while(existing.ground_triangle_ids.size() > 0)
				world.data.destroyGroundTriangle(existing.ground_triangle_ids[existing.ground_triangle_ids.size() - 1]);
			for(PointId id : old_points)
			{
				if(!world.data.isPointActive(id) || world.data.getPoint(id).referencing_chunk_count != 1) continue;
				bool used_by_component = false;
				for(PointComponentSlotId slot_id : existing.point_component_slot_ids)
					if(world.data.getPointComponentSlot(slot_id).point_id == id) { used_by_component = true; break; }
				if(!used_by_component && existing.graph_component_slot_ids.size() == 0) world.data.destroyPoint(id);
			}
		}

		if(generated_chunk.triangle_point_indices.size() > 0)
		{
			if(!world.data.isChunkActive(chunk_id)) chunk_id = world.data.createChunk(slot.world_body_id).chunk_id;
			Chunk& chunk = world.data.getChunk(chunk_id);
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
				const size_t triangle_index = index / 3;
				if(triangle_index >= generated_chunk.triangle_material_ids.size())
				{
					danikk_engine::formatFatalError("Generated planet chunk has no material for triangle %", triangle_index);
				}
				triangle.material_id = generated_chunk.triangle_material_ids[triangle_index];
				world.data.addGroundTriangleToChunk(chunk_id, triangle_id);
			}
		}
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
