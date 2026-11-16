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
	 * @brief Represents collision info data and behavior.
	 */
	struct CollisionInfo
	{
		bool collided = false;
		vec3 normal = vec3(0.0f);
		float penetration = 0.0f;
	};

	/**
	 * @brief Represents shape type data and behavior.
	 */
	struct ShapeType
	{
		static constexpr ShapeId none = 0;
		static constexpr ShapeId cube = 1;
		static constexpr ShapeId sphere = 2;
		static constexpr size_t count = 3;
	};

	/**
	 * @brief Stores mesh geometry generated for a particular shape size.
	 */
	struct ShapeMesh
	{
		vec3 size;
		Mesh* mesh;
		/**
		 * @brief Stores the generated mesh and the size for which it was built.
		 */
		ShapeMesh(vec3 size, Mesh* mesh) : size(size), mesh(mesh) {}
	};

	/**
	 * @brief Defines a shape, its mesh generator, and size-specific mesh cache.
	 */
	struct ShapeDefinition
	{
		using mesh_factory_t = Mesh* (*)(vec3 size);

		ShapeId id;
		const char* name;
		mesh_factory_t createMesh;
		DynamicArray<ShapeMesh> meshes;

		/**
		 * @brief Returns mesh entry.
		 */
		ShapeMesh* getMeshEntry(vec3 size);
	};

	extern ShapeDefinition shape_table[];
	extern const size_t shape_table_size;

	/**
	 * @brief Initializes shape table.
	 */
	void initShapeTable();

	/**
	 * @brief Finds shape.
	 */
	ShapeDefinition* findShape(ShapeId id);

	/**
	 * @brief Clears shape mesh cache.
	 */
	void clearShapeMeshCache();
}
