#pragma once

#include <danikk_framework/dynamic_array.h>
#include <danikk_engine/mesh.h>

#include <danikk_space_engine/component/point_component.h>
#include <danikk_space_engine/world/volume_cell.h>

namespace danikk_space_engine
{
	using ChunkId = uint32;
	class World;

	class Chunk
	{
		Mesh* mesh = nullptr;
		DynamicArray<uint8> shared_face_masks;

	public:
		vec3 position = vec3(0.0f);
		DynamicArray<VolumeCellId> volume_cell_ids;
		DynamicArray<PointComponentSlotId> point_component_slot_ids;

		Chunk() = default;
		Chunk(const Chunk&) = delete;
		Chunk& operator=(const Chunk&) = delete;
		Chunk(Chunk&& other) noexcept;
		Chunk& operator=(Chunk&& other) noexcept;
		~Chunk();

		Mesh& createMesh();
		void addPointComponentSlot(PointComponentSlotId slot_id);
		void tick(World& world);
		bool hasSharedFace(VolumeCellId cell_id, uint8 face_index) const;
		uint8 getSharedFaceMask(size_t cell_index) const;
	void frame(World& world);
		mat4 createWorldMatrix() const;
		void draw();
		void destroyMesh();
	};
}
