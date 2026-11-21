#include <danikk_space_engine/effects/chunk_boundaries.h>

#include <danikk_space_engine/world.h>

#include <danikk_engine/mesh.h>
#include <danikk_engine/texture_methods.h>

namespace danikk_space_engine
{
	/**
	 * @brief Batches visible chunk-boundary edge prisms and submits them as a single black draw.
	 * @param world World containing the loaded chunks to inspect.
	 * Chunks derive bounds from active point positions. Coordinates are transformed with each chunk's camera-relative
	 * world matrix. The reusable mesh is overwritten each call and empty geometry
	 * is not submitted.
	 */
	void drawChunkBoundaries(World& world)
	{
		struct ChunkPointBounds
		{
			vec3 minimum = vec3(0.0f);
			vec3 maximum = vec3(0.0f);
			bool found = false;
		};
		DynamicArray<ChunkPointBounds> chunk_bounds;
		chunk_bounds.resize(world.chunks.getChunkSlotCount());
		for(WorldBodyId body_id : world.world_body_data.getUsedWorldBodyIds())
		{
			const WorldBody& body = world.world_body_data.getWorldBody(body_id);
			for(PointId point_id : body.point_ids)
			{
				if(!world.point_data.isPointActive(point_id)) continue;
				const Point& point = world.point_data.getPoint(point_id);
				if(point.world_body_id != body_id) continue;
				for(ChunkId chunk_id : point.referencing_chunk_ids)
				{
					if(!world.chunks.isChunkActive(chunk_id)
						|| chunk_id >= chunk_bounds.size()) continue;
					const Chunk& chunk = world.chunks.getChunk(chunk_id);
					if(chunk.world_body_id != body_id) continue;
					ChunkPointBounds& bounds = chunk_bounds[chunk_id];
					if(!bounds.found)
					{
						bounds.minimum = point.position;
						bounds.maximum = point.position;
						bounds.found = true;
					}
					else
					{
						bounds.minimum = glm::min(bounds.minimum, point.position);
						bounds.maximum = glm::max(bounds.maximum, point.position);
					}
				}
			}
		}
		constexpr uint8 edge_indices[12][2]
		{
			{0, 1}, {1, 2}, {2, 3}, {3, 0},
			{4, 5}, {5, 6}, {6, 7}, {7, 4},
			{0, 4}, {1, 5}, {2, 6}, {3, 7}
		};
		static Mesh boundary_mesh;
		static DynamicArray<DefaultVertex> vertices;
		static DynamicArray<gl_point_index_t> indices;
		vertices.clear();
		indices.clear();
		constexpr float line_radius = 0.012f;
		constexpr uint8 side_triangles[4][6]
		{
			{0, 1, 5, 0, 5, 4}, {1, 2, 6, 1, 6, 5},
			{2, 3, 7, 2, 7, 6}, {3, 0, 4, 3, 4, 7}
		};

		for(WorldBodyId body_id : world.world_body_data.getUsedWorldBodyIds())
		{
			const WorldBody& body = world.world_body_data.getWorldBody(body_id);
			for(ChunkId chunk_id : body.chunk_ids)
			{
				if(chunk_id >= chunk_bounds.size()) continue;
				const Chunk& chunk = world.chunks.getChunk(chunk_id);
				const ChunkPointBounds& bounds = chunk_bounds[chunk_id];
				if(!bounds.found) continue;
				const vec3& minimum = bounds.minimum;
				const vec3& maximum = bounds.maximum;

				const vec3 corners[8]
				{
					vec3(minimum.x, minimum.y, minimum.z),
					vec3(maximum.x, minimum.y, minimum.z),
					vec3(maximum.x, maximum.y, minimum.z),
					vec3(minimum.x, maximum.y, minimum.z),
					vec3(minimum.x, minimum.y, maximum.z),
					vec3(maximum.x, minimum.y, maximum.z),
					vec3(maximum.x, maximum.y, maximum.z),
					vec3(minimum.x, maximum.y, maximum.z)
				};
				const mat4 world_matrix = chunk.createWorldMatrix(world);
				for(const auto& edge : edge_indices)
				{
					const vec3 start = vec3(world_matrix * vec4(corners[edge[0]], 1.0f));
					const vec3 end = vec3(world_matrix * vec4(corners[edge[1]], 1.0f));
					const vec3 direction = end - start;
					const float edge_length = glm::length(direction);
					if(edge_length < 0.000001f) continue;
					const vec3 axis = direction / edge_length;
					const vec3 reference = glm::abs(glm::dot(axis, vec3(0.0f, 0.0f, 1.0f))) > 0.9f
						? vec3(0.0f, 1.0f, 0.0f) : vec3(0.0f, 0.0f, 1.0f);
					const vec3 side = glm::normalize(glm::cross(axis, reference)) * line_radius;
					const vec3 up = glm::normalize(glm::cross(axis, side)) * line_radius;
					const vec3 edge_vertices[8]
					{
						start + side + up, start - side + up,
						start - side - up, start + side - up,
						end + side + up, end - side + up,
						end - side - up, end + side - up
					};
					const gl_point_index_t first_vertex = static_cast<gl_point_index_t>(vertices.size());
					for(const vec3& vertex : edge_vertices)
					{
						vertices.pushCtor(vertex, vec3(0.0f, 0.0f, 1.0f), vec2(0.0f));
					}
					for(const auto& side : side_triangles)
					{
						for(uint8 index : side) indices.push(first_vertex + index);
					}
				}
			}
		}
		if(indices.size() == 0) return;

		boundary_mesh.setData(
			reinterpret_cast<const float*>(vertices.data()),
			vertices.size() * sizeof(DefaultVertex) / sizeof(float),
			indices.data(),
			indices.size());
		DefaultVertex::setAttributes();
		white_texture.bind();
		setDrawColor(vec4(0.0f, 0.0f, 0.0f, 1.0f));
		setWorldMatrix(mat4(1.0f));
		boundary_mesh.draw();
	}
}
