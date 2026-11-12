#include <danikk_space_engine/shape/shape.h>

#include <danikk_engine/dynamic_mesh.h>
#include <danikk_framework/memory.h>

#include <cmath>
#include <limits>
#include <new>
#include <cstdlib>

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

	// Creates a unit cube mesh whose face UV ranges tile according to the requested dimensions.
	// Allocates a Mesh, emits six independently normaled quads with size-based UV extents,
	// uploads the generated vertices and indices, and configures the vertex attributes.
	static Mesh* createCubeMesh(vec3 size)
	{
		void* memory = malloc(sizeof(Mesh));
		if(memory == nullptr) std::abort();
		Mesh* result = new (memory) Mesh();

		constexpr float x = 0.5f;
		constexpr float y = 0.5f;
		constexpr float z = 0.5f;
		DynamicMesh<DefaultVertex> mesh;
		addQuad(mesh, vec3(-x,-y,-z), vec3(-x,-y, z), vec3(-x, y, z), vec3(-x, y,-z), vec3(-1,0,0), size.z, size.y);
		addQuad(mesh, vec3( x,-y, z), vec3( x,-y,-z), vec3( x, y,-z), vec3( x, y, z), vec3( 1,0,0), size.z, size.y);
		addQuad(mesh, vec3(-x,-y, z), vec3(-x,-y,-z), vec3( x,-y,-z), vec3( x,-y, z), vec3(0,-1,0), size.z, size.x);
		addQuad(mesh, vec3(-x, y,-z), vec3(-x, y, z), vec3( x, y, z), vec3( x, y,-z), vec3(0, 1,0), size.z, size.x);
		addQuad(mesh, vec3( x,-y,-z), vec3(-x,-y,-z), vec3(-x, y,-z), vec3( x, y,-z), vec3(0,0,-1), size.x, size.y);
		addQuad(mesh, vec3(-x,-y, z), vec3( x,-y, z), vec3( x, y, z), vec3(-x, y, z), vec3(0,0, 1), size.x, size.y);
		mesh.setDataToMesh(*result);
		DefaultVertex::setAttributes();
		return result;
	}

	ShapeDefinition shape_table[1];
	const size_t shape_table_size = sizeof(shape_table) / sizeof(shape_table[0]);
	static bool shape_table_initialized = false;
	static constexpr size_t shape_mesh_cache_capacity = 1024;

	void initShapeTable()
	{
		if(shape_table_initialized)
		{
			return;
		}
		shape_table[0].id = ShapeType::cube;
		shape_table[0].name = "cube";
		shape_table[0].createMesh = createCubeMesh;
		shape_table[0].meshes.reserve(shape_mesh_cache_capacity);
		shape_table_initialized = true;
	}

	ShapeMesh* ShapeDefinition::getMeshEntry(vec3 size)
	{
		for(ShapeMesh& entry : meshes)
		{
			if(entry.size.x == size.x && entry.size.y == size.y && entry.size.z == size.z)
			{
				return &entry;
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
		if(meshes.size() >= meshes.capacity()) std::abort();
		return &meshes.pushCtor(size, mesh);
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
			for(ShapeMesh& entry : shape.meshes)
			{
				entry.mesh->free();
				entry.mesh->~Mesh();
				free(entry.mesh);
			}
			shape.meshes.clear();
		}
	}
}
