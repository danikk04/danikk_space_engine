#include <limits>
#include <cstdlib>

#include <danikk_space_engine/profiler.h>
#include <danikk_framework/glm.h>

#include <danikk_space_engine/world/chunk.h>
#include <danikk_space_engine/world/world.h>

#include <danikk_engine/mesh.h>

namespace danikk_space_engine
{
	namespace
	{
		vec2 projectToUV(const vec3& position, const vec3& normal)
		{
			const vec3 absolute_normal = glm::abs(normal);
			if(absolute_normal.x >= absolute_normal.y && absolute_normal.x >= absolute_normal.z)
			{
				return vec2(position.z, position.y);
			}
			if(absolute_normal.y >= absolute_normal.z)
			{
				return vec2(position.x, position.z);
			}
			return vec2(position.x, position.y);
		}
	}

	/*
	 * Builds the chunk mesh by scanning its triangles once per loaded material and appending matching
	 * triangles contiguously, producing one index range per material for separate draw calls. While
	 * appending it skips inactive resources and degenerate triangles, computes flat face normals and
	 * projected UVs, then calculates the local bounding sphere before uploading the buffers.
	 */
	Mesh& Chunk::createMesh()
	{
		Watcher watcher("Chunk::createMesh");
		if(mesh == nullptr)
		{
			void* memory = malloc(sizeof(Mesh));
			if(memory == nullptr) std::abort();
			mesh = new (memory) Mesh();
		}

		DynamicArray<DefaultVertex> vertices;
		DynamicArray<gl_point_index_t> indices;
		material_ranges.clear();
		vertices.reserve(ground_triangle_ids.size() * 3);
		indices.reserve(ground_triangle_ids.size() * 3);

		for(MaterialId material_id = 0; material_id < getMaterialCount(); material_id++)
		{
			const uint32 first_index = static_cast<uint32>(indices.size());
			for(GroundTriangleId triangle_id : ground_triangle_ids)
			{
				if(!world.data.isGroundTriangleActive(triangle_id)) continue;
				const GroundTriangle& triangle = world.data.getGroundTriangle(triangle_id);
				if(triangle.material_id != material_id
					|| !world.data.isPointActive(triangle.points[0])
					|| !world.data.isPointActive(triangle.points[1])
					|| !world.data.isPointActive(triangle.points[2])) continue;

				if(vertices.size() + 3 > static_cast<size_t>(std::numeric_limits<gl_point_index_t>::max()) + 1)
				{
					std::abort();
				}
				const vec3 a = world.data.getPoint(triangle.points[0]).position;
				const vec3 b = world.data.getPoint(triangle.points[1]).position;
				const vec3 c = world.data.getPoint(triangle.points[2]).position;
				const vec3 cross_product = glm::cross(b - a, c - a);
				const float normal_length = glm::length(cross_product);
				if(normal_length <= 0.000001f) continue;
				const vec3 normal = cross_product / normal_length;
				const vec3 positions[3] = {a, b, c};
				for(uint8 corner = 0; corner < 3; corner++)
				{
					indices.push(static_cast<gl_point_index_t>(vertices.size()));
					vertices.pushCtor(positions[corner], normal, projectToUV(positions[corner], normal));
				}
			}
			const uint32 index_count = static_cast<uint32>(indices.size()) - first_index;
			if(index_count > 0) material_ranges.push(ChunkMaterialRange{material_id, first_index, index_count});
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
			{
				bounds_radius = glm::max(bounds_radius, glm::length(vertex.pos - bounds_center));
			}
			bounds_radius += 0.0001f;
			bounds_valid = true;
		}

		mesh->setData(
			reinterpret_cast<const float*>(vertices.data()),
			vertices.size() * sizeof(DefaultVertex) / sizeof(float),
			indices.data(),
			indices.size());
		DefaultVertex::setAttributes();
		mesh_dirty = false;
		return *mesh;
	}
}
