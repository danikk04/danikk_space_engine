#pragma once

#include <danikk_space_engine/default.h>
#include <danikk_engine/mesh.h>
#include <danikk_framework/dynamic_array.h>

#include <cstddef>
#include <cstdint>

namespace danikk_space_engine
{
	enum class ShapeId : uint8_t
	{
		cube
	};

	struct ShapeMeshCacheEntry
	{
		vec3 size;
		Mesh* mesh;
	};

	struct ShapeDefinition
	{
		using mesh_factory_t = Mesh* (*)(vec3 size);

		ShapeId id;
		const char* name;
		mesh_factory_t createMesh;
		DynamicArray<ShapeMeshCacheEntry> meshes;

		Mesh* getMesh(vec3 size);
	};

	extern ShapeDefinition shape_table[];
	extern const size_t shape_table_size;

	void initShapeTable();
	ShapeDefinition* findShape(ShapeId id);
	void clearShapeMeshCache();
}
