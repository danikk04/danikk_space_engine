#include <cstdlib>
#include <limits>

#include <danikk_space_engine/profiler.h>
#include <danikk_space_engine/world/chunk.h>

#include <danikk_engine/mesh.h>
#include <danikk_framework/glm.h>

namespace danikk_space_engine
{
	namespace
	{
		/** @brief Projects a chunk-local vertex onto a planar UV basis chosen by its face normal. */
		vec2 projectToUV(const vec3& position, const vec3& normal)
		{
			const vec3 absolute_normal = glm::abs(normal);
			if(absolute_normal.x >= absolute_normal.y && absolute_normal.x >= absolute_normal.z)
				return vec2(position.z, position.y);
			if(absolute_normal.y >= absolute_normal.z) return vec2(position.x, position.z);
			return vec2(position.x, position.y);
		}
	}

	/**
	 * @brief Builds and uploads a material-grouped GPU mesh from a temporary generated buffer.
	 * @param positions Source positions, valid only for the duration of this call.
	 * @param position_count Number of source positions.
	 * @param triangle_indices Three indices per triangle.
	 * @param index_count Number of indices.
	 * @param triangle_material_ids Material ID for each triangle.
	 * @param material_count Number of material IDs.
	 * The method expands indexed triangles into flat-shaded vertices, calculates UVs
	 * from the dominant normal axis, records material draw ranges, computes a local
	 * bounding sphere, and uploads the result. No CPU copy of terrain triangles or
	 * vertices is retained by the chunk.
	 */
	void Chunk::createMesh(const vec3* positions, size_t position_count,
		const uint32* triangle_indices, size_t index_count,
		const MaterialId* triangle_material_ids, size_t material_count,
		bool is_planet_terrain, bool is_build_part_mesh)
	{
		DSE_WATCH();
		if(index_count % 3 != 0 || material_count != index_count / 3
			|| (index_count > 0 && (positions == nullptr || triangle_indices == nullptr || triangle_material_ids == nullptr)))
		{
			danikk_engine::formatFatalError("Invalid temporary chunk mesh buffer");
		}
		if(mesh == nullptr)
		{
			void* memory = malloc(sizeof(Mesh));
			if(memory == nullptr) std::abort();
			mesh = new (memory) Mesh();
		}

		DynamicArray<DefaultVertex> vertices;
		DynamicArray<gl_point_index_t> indices;
		material_ranges.clear();
		vertices.reserve(index_count);
		indices.reserve(index_count);

		for(MaterialId material_id = 0; material_id < getMaterialCount(); material_id++)
		{
			const uint32 first_index = static_cast<uint32>(indices.size());
			for(size_t triangle = 0; triangle < material_count; triangle++)
			{
				if(triangle_material_ids[triangle] != material_id) continue;
				const uint32 source_indices[3] =
				{
					triangle_indices[triangle * 3],
					triangle_indices[triangle * 3 + 1],
					triangle_indices[triangle * 3 + 2]
				};
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
				for(uint8 corner = 0; corner < 3; corner++)
				{
					indices.push(static_cast<gl_point_index_t>(vertices.size()));
					vertices.pushCtor(triangle_positions[corner], normal, projectToUV(triangle_positions[corner], normal));
				}
			}
			const uint32 index_count_for_material = static_cast<uint32>(indices.size()) - first_index;
			if(index_count_for_material > 0)
				material_ranges.push(ChunkMaterialRange{material_id, first_index, index_count_for_material});
		}

		if(vertices.size() == 0)
		{
			bounds_center = vec3(0.0f);
			bounds_radius = 0.0f;
			bounds_valid = false;
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
			bounds_center = (minimum + maximum) * 0.5f;
			bounds_radius = 0.0f;
			for(const DefaultVertex& vertex : vertices)
				bounds_radius = glm::max(bounds_radius, glm::length(vertex.pos - bounds_center));
			bounds_radius += 0.0001f;
			bounds_valid = true;
		}

		mesh->setData(
			reinterpret_cast<const float*>(vertices.data()),
			vertices.size() * sizeof(DefaultVertex) / sizeof(float),
			indices.data(),
			indices.size());
		DefaultVertex::setAttributes();
		planet_terrain = is_planet_terrain;
		has_build_part_mesh = is_build_part_mesh;
		build_part_mesh_dirty = false;
	}
}
