#include <danikk_space_engine/chunk.h>
#include <danikk_space_engine/world.h>

#include <danikk_engine/mesh.h>
#include <danikk_engine/texture_methods.h>
#include <danikk_framework/glm.h>

#include <cstdlib>
#include <limits>
#include <new>

namespace danikk_space_engine
{
	static vec2 projectToUV(const vec3& position, const vec3& normal)
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

	Chunk::Chunk(Chunk&& other) noexcept
		: mesh(other.mesh), position(other.position), volume_cell_ids(static_cast<DynamicArray<VolumeCellId>&&>(other.volume_cell_ids))
	{
		other.mesh = nullptr;
	}

	Chunk& Chunk::operator=(Chunk&& other) noexcept
	{
		if(this == &other)
		{
			return *this;
		}

		destroyMesh();
		position = other.position;
		volume_cell_ids = static_cast<DynamicArray<VolumeCellId>&&>(other.volume_cell_ids);
		mesh = other.mesh;
		other.mesh = nullptr;
		return *this;
	}

	Chunk::~Chunk()
	{
		destroyMesh();
	}

	void Chunk::destroyMesh()
	{
		if(mesh == nullptr)
		{
			return;
		}

		mesh->free();
		mesh->~Mesh();
		free(mesh);
		mesh = nullptr;
	}

	// Builds a triangle mesh for all valid volume cells owned by this chunk.
	// Expands topology triangles into vertices, skips invalid or degenerate faces, projects
	// planar UVs from the face normal, uploads buffers, and configures vertex attributes.
	Mesh& Chunk::createMesh()
	{
		if(mesh == nullptr)
		{
			void* memory = malloc(sizeof(Mesh));
			if(memory == nullptr) std::abort();
			mesh = new (memory) Mesh();
		}

		DynamicArray<DefaultVertex> vertices;
		DynamicArray<gl_point_index_t> indices;

		for(VolumeCellId cell_id : volume_cell_ids)
		{
			if(cell_id >= world.data.getVolumeCellCount())
			{
				std::abort();
			}
			const VolumeCell& cell = world.data.getVolumeCell(cell_id);
			const VolumeCellTopologyDefinition& topology = getVolumeCellTopologyDefinition(cell.topology);
			for(uint8 triangle_index = 0; triangle_index < topology.triangle_count; triangle_index++)
			{
				const VolumeCellTriangle& triangle = topology.triangles[triangle_index];
				const PointId point_ids[3]
				{
					cell.points[triangle.points[0]],
					cell.points[triangle.points[1]],
					cell.points[triangle.points[2]]
				};

				if(point_ids[0] >= world.data.getPointCount() || point_ids[1] >= world.data.getPointCount() || point_ids[2] >= world.data.getPointCount())
				{
					std::abort();
				}

				const vec3 a = world.data.getPoint(point_ids[0]).position;
				const vec3 b = world.data.getPoint(point_ids[1]).position;
				const vec3 c = world.data.getPoint(point_ids[2]).position;
				const vec3 cross_product = glm::cross(b - a, c - a);
				const float normal_length = glm::length(cross_product);
				if(normal_length <= 0.000001f)
				{
					continue;
				}

				if(vertices.size() + 3 > static_cast<size_t>(std::numeric_limits<gl_point_index_t>::max()) + 1)
				{
					std::abort();
				}

				const vec3 normal = cross_product / normal_length;
				const vec3 triangle_positions[3] = {a, b, c};
				const vec2 triangle_uvs[3] =
				{
					projectToUV(a, normal),
					projectToUV(b, normal),
					projectToUV(c, normal)
				};

				for(uint8 vertex_index = 0; vertex_index < 3; vertex_index++)
				{
					indices.push(static_cast<gl_point_index_t>(vertices.size()));
					vertices.pushCtor(triangle_positions[vertex_index], normal, triangle_uvs[vertex_index]);
				}
			}
		}

		mesh->setData(
			reinterpret_cast<const float*>(vertices.data()),
			vertices.size() * sizeof(DefaultVertex) / sizeof(float),
			indices.data(),
			indices.size());
		DefaultVertex::setAttributes();
		return *mesh;
	}

	mat4 Chunk::createWorldMatrix() const
	{
		return glm::translate(mat4(1.0f), position);
	}

	void Chunk::draw()
	{
		if(mesh == nullptr || volume_cell_ids.size() == 0)
		{
			return;
		}

		const VolumeCellId cell_id = volume_cell_ids[0];
		if(cell_id >= world.data.getVolumeCellCount())
		{
			return;
		}
		MaterialDefinition& material = getMaterial(world.data.getVolumeCell(cell_id).material);
		if(material.texture.isNull())
		{
			white_texture.bind();
		}
		else
		{
			material.texture.bind();
		}

		setDrawColor(vec4(material.color, 1.0f));
		setWorldMatrix(createWorldMatrix());
		mesh->draw();
	}
}
