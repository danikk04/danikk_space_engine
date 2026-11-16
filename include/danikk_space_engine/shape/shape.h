#pragma once

#include <danikk_space_engine/default.h>
#include <danikk_space_engine/id.h>
#include <danikk_engine/mesh.h>
#include <danikk_framework/dynamic_array.h>

#include <cstddef>
#include <cstdint>

namespace danikk_space_engine
{

	/**
	 * @brief Stores a shape-pair overlap result and its minimum-translation contact estimate.
	 * This belongs to the legacy GameObject collision API and is not used by the active WorldPhysics system.
	 */
	struct CollisionInfo
	{
		/** @brief True when the shape pair overlaps. */
		bool collided = false;
		/** @brief Contact direction used to separate the pair when collided is true. */
		vec3 normal = vec3(0.0f);
		/** @brief Estimated overlap depth along normal. */
		float penetration = 0.0f;
	};

	/**
	 * @brief Defines legacy shape IDs and the dimensions of shape-indexed dispatch tables.
	 */
	struct ShapeType
	{
		/** @brief Sentinel for no shape; dispatches to no-collision behavior. */
		static constexpr ShapeId none = 0;
		/** @brief Axis-aligned unit cube mesh/collision kind. */
		static constexpr ShapeId cube = 1;
		/** @brief Legacy collision-shape ID for a sphere. */
		static constexpr ShapeId sphere = 2;
		/** @brief Number of slots in tables indexed by ShapeId, including none. */
		static constexpr size_t count = 3;
	};

	/**
	 * @brief Associates one exact requested shape size with its generated mesh pointer.
	 * Mesh ownership is released by clearShapeMeshCache().
	 */
	struct ShapeMesh
	{
		/** @brief Exact requested dimensions used as this cache entry's lookup key. */
		vec3 size;
		/** @brief Heap-allocated mesh released by clearShapeMeshCache(). */
		Mesh* mesh;
		/**
		 * @brief Associates requested dimensions with their generated mesh.
		 * @param size Exact dimensions used for later cache lookup.
		 * @param mesh Mesh whose ownership transfers to this cache entry.
		 */
		ShapeMesh(vec3 size, Mesh* mesh) : size(size), mesh(mesh) {}
	};

	/**
	 * @brief Defines a shape mesh factory and caches generated meshes by exact dimensions.
	 */
	struct ShapeDefinition
	{
		/** @brief Function pointer used to create a mesh for a requested size. */
		using mesh_factory_t = Mesh* (*)(vec3 size);

		/** @brief Shape's stable ID used to find this table entry. */
		ShapeId id;
		/** @brief Human-readable shape name. */
		const char* name;
		/** @brief Factory returning a heap-allocated mesh for the requested dimensions. */
		mesh_factory_t createMesh;
		/** @brief Generated mesh cache; the current implementation reserves a fixed capacity. */
		DynamicArray<ShapeMesh> meshes;

		/**
		 * @brief Returns a cached mesh for exact dimensions or generates and caches one on a miss.
		 * @param size Requested dimensions compared component-wise using exact float equality.
		 * @return Cache entry, or nullptr when the factory is absent or returns nullptr.
		 * The cache has a fixed reserved capacity; exhausting it aborts rather than
		 * growing the array in this legacy implementation.
		 */
		ShapeMesh* getMeshEntry(vec3 size);
	};

	/** @brief Legacy shape definitions registered by initShapeTable(). */
	extern ShapeDefinition shape_table[];
	/** @brief Number of statically allocated definitions in shape_table. */
	extern const size_t shape_table_size;

	/**
	 * @brief Initializes the legacy shape table once and registers the cube mesh factory.
	 */
	void initShapeTable();

	/**
	 * @brief Finds a shape definition by linear ID lookup.
	 * @param id Shape ID to locate.
	 * @return Pointer to the table entry, or nullptr when no entry matches.
	 */
	ShapeDefinition* findShape(ShapeId id);

	/**
	 * @brief Frees and destroys every cached mesh, then clears each shape's cache array.
	 * Meshes are released with their engine cleanup function, explicit destructor,
	 * and free(); call while the graphics resources are still valid.
	 */
	void clearShapeMeshCache();
}
