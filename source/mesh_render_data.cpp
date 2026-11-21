#include <cstdlib>
#include <limits>
#include <memory>
#include <new>

#include <danikk_space_engine/mesh_render_data.h>
#include <danikk_space_engine/material.h>
#include <danikk_space_engine/profiler.h>

#include <danikk_engine/danikk_engine.h>
#include <danikk_engine/texture_methods.h>

namespace danikk_space_engine
{
	namespace
	{
		/** @brief Projects a vertex onto a planar UV basis selected by its face normal. */
		vec2 projectToUV(const vec3& position, const vec3& normal)
		{
			const vec3 absolute_normal = glm::abs(normal);
			if(absolute_normal.x >= absolute_normal.y && absolute_normal.x >= absolute_normal.z)
				return vec2(position.z, position.y);
			if(absolute_normal.y >= absolute_normal.z) return vec2(position.x, position.z);
			return vec2(position.x, position.y);
		}
	}

	void createMeshRenderData(MeshRenderData& record, const vec3* positions, size_t position_count,
		const uint32* triangle_indices, size_t index_count,
		const MaterialId* triangle_material_ids, size_t material_count)
	{
		DSE_WATCH();
		if(index_count % 3 != 0 || material_count != index_count / 3
			|| (index_count > 0 && (positions == nullptr || triangle_indices == nullptr || triangle_material_ids == nullptr)))
			danikk_engine::formatFatalError("Invalid temporary mesh buffer");
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
		for(MaterialId material_id = 0; material_id < getMaterialCount(); ++material_id)
		{
			const uint32 first_index = static_cast<uint32>(indices.size());
			for(size_t triangle = 0; triangle < material_count; ++triangle)
			{
				if(triangle_material_ids[triangle] != material_id) continue;
				const uint32 source_indices[3] = {
					triangle_indices[triangle * 3], triangle_indices[triangle * 3 + 1], triangle_indices[triangle * 3 + 2]};
				if(source_indices[0] >= position_count || source_indices[1] >= position_count || source_indices[2] >= position_count)
					danikk_engine::formatFatalError("Generated mesh has an invalid vertex index");
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
				record.material_ranges.push(MeshMaterialRange{material_id, first_index, range_index_count});
		}
		record.mesh->setData(reinterpret_cast<const float*>(vertices.data()),
			vertices.size() * sizeof(DefaultVertex) / sizeof(float), indices.data(), indices.size());
		DefaultVertex::setAttributes();
	}

	MeshDrawResult drawMeshRenderData(const MeshRenderData& record, const mat4& world_matrix)
	{
		DSE_WATCH();
		if(record.mesh == nullptr) return MeshDrawResult::missing_mesh;
		if(record.material_ranges.size() == 0) return MeshDrawResult::empty_mesh;
		setWorldMatrix(world_matrix);
		bool drew_range = false;
		for(const MeshMaterialRange& range : record.material_ranges)
		{
			if(range.index_count == 0) continue;
			MaterialDefinition& material = getMaterial(range.material_id);
			if(material.texture.isNull()) white_texture.bind();
			else material.texture.bind();
			setDrawColor(vec4(material.color, 1.0f));
			record.mesh->drawRange(range.first_index, range.index_count);
			drew_range = true;
		}
		return drew_range ? MeshDrawResult::drawn : MeshDrawResult::empty_mesh;
	}

	void destroyMeshRenderData(MeshRenderData& record)
	{
		DSE_WATCH();
		if(record.mesh != nullptr)
		{
			record.mesh->free();
			std::destroy_at(record.mesh);
			free(record.mesh);
			record.mesh = nullptr;
		}
		record.material_ranges.clear();
	}
}
