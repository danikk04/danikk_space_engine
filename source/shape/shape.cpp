#include <danikk_space_engine/shape/shape.h>

#include <danikk_engine/dynamic_mesh.h>
#include <danikk_framework/memory.h>

#include <cmath>
#include <limits>
#include <new>
#include <cstdlib>

namespace danikk_space_engine
{

	/**
	 * @brief Appends a four-vertex face with UV extents scaled for texture tiling.
	 * @param mesh Dynamic mesh receiving the two face triangles.
	 * @param a First face corner.
	 * @param b Second face corner.
	 * @param c Third face corner.
	 * @param d Fourth face corner.
	 * @param normal Flat face normal shared by all corners.
	 * @param u_size Horizontal UV extent, normally the face's first physical dimension.
	 * @param v_size Vertical UV extent, normally the face's second physical dimension.
	 */
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

	/**
	 * @brief Allocates a centered unit cube mesh and tiles each face UV by the requested size.
	 * @param size Dimensions that scale the per-face UV ranges; generated geometry remains unit-sized.
	 * @return Heap-allocated Mesh owned by the shape cache after successful generation.
	 */
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

	/**
	 * @brief Registers the cube definition once and reserves space for up to 1024 cached meshes.
	 */
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

	/**
	 * @brief Performs an exact-size linear cache lookup, creating and storing a mesh on a miss.
	 * @param size Requested mesh dimensions.
	 * @return Pointer to the existing or newly appended entry, or nullptr if no factory is available.
	 * The implementation aborts when the pre-reserved cache is full; it does not
	 * resize the cache dynamically.
	 */
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

	/**
	 * @brief Returns the first legacy shape-table entry with the requested ID.
	 * @param id Shape ID to find.
	 * @return Matching definition or nullptr when absent.
	 */
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

	/**
	 * @brief Releases every cached Mesh object and resets each definition's mesh list.
	 */
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
