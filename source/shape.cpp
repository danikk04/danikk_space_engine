#include <danikk_space_engine/shape.h>

#include <danikk_engine/dynamic_mesh.h>
#include <danikk_framework/memory.h>

#include <new>

namespace danikk_space_engine
{
	static void addQuad(DynamicMesh<DefaultVertex>& mesh, const vec3& a, const vec3& b,
			const vec3& c, const vec3& d, const vec3& normal, float u_size, float v_size)
	{
		DefaultVertex vertices[4]
		{
			DefaultVertex(a, normal, vec2(0.0f, 0.0f)),
			DefaultVertex(b, normal, vec2(u_size, 0.0f)),
			DefaultVertex(c, normal, vec2(u_size, v_size)),
			DefaultVertex(d, normal, vec2(0.0f, v_size))
		};
		mesh.addSquare(vertices);
	}

	static Mesh* createCubeMesh(vec3 size)
	{
		void* memory = malloc(sizeof(Mesh));
		if(memory == nullptr)
		{
			return nullptr;
		}
		Mesh* result = new (memory) Mesh();

		const float x = size.x * 0.5f;
		const float y = size.y * 0.5f;
		const float z = size.z * 0.5f;
		DynamicMesh<DefaultVertex> mesh;
		addQuad(mesh, vec3(-x,-y,-z), vec3(-x,-y, z), vec3(-x, y, z), vec3(-x, y,-z), vec3(-1,0,0), size.z, size.y);
		addQuad(mesh, vec3( x,-y, z), vec3( x,-y,-z), vec3( x, y,-z), vec3( x, y, z), vec3( 1,0,0), size.z, size.y);
		addQuad(mesh, vec3(-x,-y, z), vec3( x,-y, z), vec3( x,-y,-z), vec3(-x,-y,-z), vec3(0,-1,0), size.x, size.z);
		addQuad(mesh, vec3(-x, y,-z), vec3( x, y,-z), vec3( x, y, z), vec3(-x, y, z), vec3(0, 1,0), size.x, size.z);
		addQuad(mesh, vec3(-x,-y,-z), vec3( x,-y,-z), vec3( x, y,-z), vec3(-x, y,-z), vec3(0,0,-1), size.x, size.y);
		addQuad(mesh, vec3( x,-y, z), vec3(-x,-y, z), vec3(-x, y, z), vec3( x, y, z), vec3(0,0, 1), size.x, size.y);
		DefaultVertex::setAttributes();
		mesh.setDataToMesh(*result);
		return result;
	}

	ShapeDefinition shape_table[1];
	const size_t shape_table_size = sizeof(shape_table) / sizeof(shape_table[0]);
	static bool shape_table_initialized = false;

	void initShapeTable()
	{
		if(shape_table_initialized)
		{
			return;
		}
		shape_table[0].id = ShapeId::cube;
		shape_table[0].name = "cube";
		shape_table[0].createMesh = createCubeMesh;
		shape_table_initialized = true;
	}

	Mesh* ShapeDefinition::getMesh(vec3 size)
	{
		for(const ShapeMeshCacheEntry& entry : meshes)
		{
			if(entry.size.x == size.x && entry.size.y == size.y && entry.size.z == size.z)
			{
				return entry.mesh;
			}
		}

		if(createMesh == nullptr)
		{
			return nullptr;
		}
		Mesh* mesh = createMesh(size);
		if(mesh == nullptr)
		{
			return nullptr;
		}
		meshes.push(ShapeMeshCacheEntry{size, mesh});
		return mesh;
	}

	ShapeDefinition* findShape(ShapeId id)
	{
		for(size_t index = 0; index < shape_table_size; ++index)
		{
			if(shape_table[index].id == id)
			{
				return &shape_table[index];
			}
		}
		return nullptr;
	}

	void clearShapeMeshCache()
	{
		for(ShapeDefinition& shape : shape_table)
		{
			for(ShapeMeshCacheEntry& entry : shape.meshes)
			{
				entry.mesh->free();
				entry.mesh->~Mesh();
				free(entry.mesh);
			}
			shape.meshes.clear();
		}
	}
}
