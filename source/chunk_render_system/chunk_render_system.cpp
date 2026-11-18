#include <cstdlib>
#include <limits>
#include <memory>
#include <new>

#include <danikk_space_engine/profiler.h>
#include <danikk_space_engine/chunk_render_system.h>
#include <danikk_space_engine/world/world.h>

#include <danikk_engine/texture_methods.h>
#include <danikk_framework/glm.h>

namespace danikk_space_engine
{
	namespace
	{
		/** @brief Projects a chunk-local vertex onto a planar UV basis chosen by its face normal. */
		vec2 projectToUV(const vec3& position, const vec3& normal)
		{
			DSE_WATCH();
			const vec3 absolute_normal = glm::abs(normal);
			if(absolute_normal.x >= absolute_normal.y && absolute_normal.x >= absolute_normal.z)
				return vec2(position.z, position.y);
			if(absolute_normal.y >= absolute_normal.z) return vec2(position.x, position.z);
			return vec2(position.x, position.y);
		}
	}

	ChunkRenderRecord& ChunkRenderSystem::getRecord(ChunkId chunk_id)
	{
		DSE_WATCH();
		if(records.size() <= chunk_id) records.resize(static_cast<size_t>(chunk_id) + 1);
		return records[chunk_id];
	}

	const ChunkRenderRecord* ChunkRenderSystem::findRecord(ChunkId chunk_id) const
	{
		DSE_WATCH();
		return chunk_id < records.size() ? &records[chunk_id] : nullptr;
	}

	/**
	 * @brief Converts transient indexed triangles into a flat-shaded, material-grouped GPU mesh.
	 *
	 * The method validates input indices, duplicates vertices per triangle, derives UVs from each
	 * face normal, calculates local bounds for raycast broadphase, then uploads the arrays. CPU
	 * geometry remains owned by the caller and is not retained.
	 */
	void ChunkRenderSystem::createMesh(Chunk& chunk, const vec3* positions, size_t position_count,
		const uint32* triangle_indices, size_t index_count,
		const MaterialId* triangle_material_ids, size_t material_count,
		bool is_planet_terrain, bool is_build_part_mesh)
	{
		DSE_WATCH();
		if(index_count % 3 != 0 || material_count != index_count / 3
			|| (index_count > 0 && (positions == nullptr || triangle_indices == nullptr || triangle_material_ids == nullptr)))
			danikk_engine::formatFatalError("Invalid temporary chunk mesh buffer");

		ChunkRenderRecord& record = getRecord(chunk.chunk_id);
		if(record.mesh == nullptr)
		{
			void* memory = malloc(sizeof(Mesh));
			if(memory == nullptr) std::abort();
			record.mesh = new (memory) Mesh();
		}

		DynamicArray<DefaultVertex> vertices;
		DynamicArray<gl_point_index_t> indices;
		record.material_ranges.clear();
		vertices.reserve(index_count);
		indices.reserve(index_count);

		for(MaterialId material_id = 0; material_id < getMaterialCount(); material_id++)
		{
			const uint32 first_index = static_cast<uint32>(indices.size());
			for(size_t triangle = 0; triangle < material_count; triangle++)
			{
				if(triangle_material_ids[triangle] != material_id) continue;
				const uint32 source_indices[3] = {
					triangle_indices[triangle * 3], triangle_indices[triangle * 3 + 1], triangle_indices[triangle * 3 + 2]};
				if(source_indices[0] >= position_count || source_indices[1] >= position_count || source_indices[2] >= position_count)
					danikk_engine::formatFatalError("Generated chunk mesh has an invalid vertex index");
				const vec3& a = positions[source_indices[0]];
				const vec3& b = positions[source_indices[1]];
				const vec3& c = positions[source_indices[2]];
				const vec3 cross_product = glm::cross(b - a, c - a);
				const float normal_length = glm::length(cross_product);
				if(normal_length <= 0.000001f) continue;
				if(vertices.size() + 3 > static_cast<size_t>(std::numeric_limits<gl_point_index_t>::max()) + 1)
					std::abort();
				const vec3 normal = cross_product / normal_length;
				const vec3 triangle_positions[3] = {a, b, c};
				for(const vec3& position : triangle_positions)
				{
					indices.push(static_cast<gl_point_index_t>(vertices.size()));
					vertices.pushCtor(position, normal, projectToUV(position, normal));
				}
			}
			const uint32 range_index_count = static_cast<uint32>(indices.size()) - first_index;
			if(range_index_count > 0)
				record.material_ranges.push(ChunkMaterialRange{material_id, first_index, range_index_count});
		}

		if(vertices.size() == 0)
		{
			chunk.bounds_center = vec3(0.0f);
			chunk.bounds_radius = 0.0f;
			chunk.bounds_valid = false;
		}
		else
		{
			vec3 minimum = vertices[0].pos;
			vec3 maximum = vertices[0].pos;
			for(const DefaultVertex& vertex : vertices)
			{
				minimum = glm::min(minimum, vertex.pos);
				maximum = glm::max(maximum, vertex.pos);
			}
			chunk.bounds_center = (minimum + maximum) * 0.5f;
			chunk.bounds_radius = 0.0f;
			for(const DefaultVertex& vertex : vertices)
				chunk.bounds_radius = glm::max(chunk.bounds_radius, glm::length(vertex.pos - chunk.bounds_center));
			chunk.bounds_radius += 0.0001f;
			chunk.bounds_valid = true;
		}

		record.mesh->setData(
			reinterpret_cast<const float*>(vertices.data()),
			vertices.size() * sizeof(DefaultVertex) / sizeof(float), indices.data(), indices.size());
		DefaultVertex::setAttributes();
		chunk.planet_terrain = is_planet_terrain;
		record.has_build_part_mesh = is_build_part_mesh;
	}

	/** @brief Draws each non-empty material range using the owning chunk's transform. */
	void ChunkRenderSystem::draw(const Chunk& chunk, World& world)
	{
		DSE_WATCH();
		const ChunkRenderRecord* record = findRecord(chunk.chunk_id);
		if(record == nullptr || record->mesh == nullptr || record->material_ranges.size() == 0) return;
		setWorldMatrix(chunk.createWorldMatrix(world));
		for(const ChunkMaterialRange& range : record->material_ranges)
		{
			if(range.index_count == 0) continue;
			MaterialDefinition& material = getMaterial(range.material_id);
			if(material.texture.isNull()) white_texture.bind();
			else material.texture.bind();
			setDrawColor(vec4(material.color, 1.0f));
			record->mesh->drawRange(range.first_index, range.index_count);
		}
	}

	void ChunkRenderSystem::destroyChunk(Chunk& chunk)
	{
		DSE_WATCH();
		const ChunkId chunk_id = chunk.chunk_id;
		if(chunk_id < records.size())
		{
			ChunkRenderRecord& record = records[chunk_id];
			if(record.mesh != nullptr)
			{
				record.mesh->free();
				std::destroy_at(record.mesh);
				free(record.mesh);
				record.mesh = nullptr;
			}
			record.material_ranges.clear();
			record.has_build_part_mesh = false;
		}
		chunk.bounds_valid = false;
		chunk.bounds_center = vec3(0.0f);
		chunk.bounds_radius = 0.0f;
		chunk.planet_terrain = false;
	}

	void ChunkRenderSystem::clear()
	{
		DSE_WATCH();
		for(ChunkRenderRecord& record : records)
		{
			if(record.mesh == nullptr) continue;
			record.mesh->free();
			std::destroy_at(record.mesh);
			free(record.mesh);
			record.mesh = nullptr;
			record.material_ranges.clear();
			record.has_build_part_mesh = false;
		}
		records.clear();
	}

	bool ChunkRenderSystem::hasBuildPartMesh(ChunkId chunk_id) const
	{
		DSE_WATCH();
		const ChunkRenderRecord* record = findRecord(chunk_id);
		return record != nullptr && record->has_build_part_mesh;
	}
}
