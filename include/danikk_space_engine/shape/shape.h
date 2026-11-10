#pragma once

#include <danikk_space_engine/default.h>
#include <danikk_engine/mesh.h>
#include <danikk_framework/dynamic_array.h>

#include <cstddef>
#include <cstdint>

namespace danikk_space_engine
{
	struct CollisionInfo
	{
		bool collided = false;
		vec3 normal = vec3(0.0f);
		float penetration = 0.0f;
	};

	typedef uint32 ShapeId;

	struct ShapeType
	{
		static constexpr ShapeId none = 0;
		static constexpr ShapeId cube = 1;
		static constexpr ShapeId sphere = 2;
		static constexpr size_t count = 3;
	};

	struct ShapeMesh
	{
		vec3 size;
		Mesh* mesh;

		ShapeMesh(vec3 size, Mesh* mesh) : size(size), mesh(mesh) {}
	};

	struct ShapeDefinition
	{
		using mesh_factory_t = Mesh* (*)(vec3 size);

		ShapeId id;
		const char* name;
		mesh_factory_t createMesh;
		DynamicArray<ShapeMesh> meshes;

		ShapeMesh* getMeshEntry(vec3 size);
	};

	extern ShapeDefinition shape_table[];
	extern const size_t shape_table_size;

	void initShapeTable();
	ShapeDefinition* findShape(ShapeId id);
	void clearShapeMeshCache();
}
