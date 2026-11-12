#pragma once

#include <danikk_framework/dynamic_array.h>
#include <danikk_engine/mesh.h>

#include <danikk_space_engine/volume_cell.h>

namespace danikk_space_engine
{
	using RegionId = uint32;

	class Region
	{
		Mesh* mesh = nullptr;

	public:
		vec3 position = vec3(0.0f);
		DynamicArray<VolumeCellId> volume_cell_ids;

		Region() = default;
		Region(const Region&) = delete;
		Region& operator=(const Region&) = delete;
		Region(Region&& other) noexcept;
		Region& operator=(Region&& other) noexcept;
		~Region();

		Mesh& createMesh();
		mat4 createWorldMatrix() const;
		void draw();
		void destroyMesh();
	};
}
