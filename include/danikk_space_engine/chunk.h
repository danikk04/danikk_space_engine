#pragma once

#include <danikk_framework/dynamic_array.h>
#include <danikk_engine/mesh.h>

#include <danikk_space_engine/volume_cell.h>

namespace danikk_space_engine
{
	using ChunkId = uint32;

	class Chunk
	{
		Mesh* mesh = nullptr;

	public:
		vec3 position = vec3(0.0f);
		DynamicArray<VolumeCellId> volume_cell_ids;

		Chunk() = default;
		Chunk(const Chunk&) = delete;
		Chunk& operator=(const Chunk&) = delete;
		Chunk(Chunk&& other) noexcept;
		Chunk& operator=(Chunk&& other) noexcept;
		~Chunk();

		Mesh& createMesh();
		mat4 createWorldMatrix() const;
		void draw();
		void destroyMesh();
	};
}
