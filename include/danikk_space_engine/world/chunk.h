#pragma once

#include <danikk_framework/dynamic_array.h>
#include <danikk_engine/mesh.h>

#include <danikk_space_engine/component/point/point.h>
#include <danikk_space_engine/component/graph/graph.h>
#include <danikk_space_engine/world/ground_triangle.h>
#include <danikk_space_engine/id.h>
#include <danikk_space_engine/material.h>
#include <danikk_space_engine/fwd.h>

namespace danikk_space_engine
{

	/**
	 * @brief Represents chunk material range data and behavior.
	 */
	struct ChunkMaterialRange
	{
		MaterialId material_id = default_materials::void_material;
		uint32 first_index = 0;
		uint32 index_count = 0;
	};

	/**
	 * @brief Stores local geometry, components, and the mesh for a region of a world body.
	 */
	class Chunk
	{
		Mesh* mesh = nullptr;
		DynamicArray<ChunkMaterialRange> material_ranges;
		bool bounds_valid = false;

	public:
		bool mesh_dirty = true;
		vec3 bounds_center = vec3(0.0f);
		float bounds_radius = 0.0f;
		ChunkId chunk_id = 0;
		WorldBodyId world_body_id = 0;
		glm::ivec3 grid_coordinate = glm::ivec3(0);
		glm::dvec3 position = glm::dvec3(0.0);
		DynamicArray<GroundTriangleId> ground_triangle_ids;
		DynamicArray<PointId> point_ids;
		DynamicArray<PointComponentSlotId> point_component_slot_ids;
		DynamicArray<GraphComponentSlotId> graph_component_slot_ids;

		Chunk() = default;
		Chunk(const Chunk&) = delete;
		Chunk& operator=(const Chunk&) = delete;
		Chunk(Chunk&& other) noexcept;
		Chunk& operator=(Chunk&& other) noexcept;
		~Chunk();

		/**
		 * @brief Creates mesh.
		 */
		Mesh& createMesh();

		/**
		 * @brief Adds point component slot.
		 */
		void addPointComponentSlot(PointComponentSlotId slot_id);

		/**
		 * @brief Adds graph component slot.
		 */
		void addGraphComponentSlot(GraphComponentSlotId slot_id);

		/**
		 * @brief Performs the tick operation.
		 */
		void tick(World& world);

		/**
		 * @brief Performs the frame operation.
		 */
		void frame(World& world);

		/**
		 * @brief Creates world matrix.
		 */
		mat4 createWorldMatrix(const World& world) const;

		/**
		 * @brief Performs the draw operation.
		 */
		void draw(World& world);

		/**
		 * @brief Removes and releases mesh.
		 */
		void destroyMesh();

		/**
		 * @brief Invalidates mesh.
		 */
		void invalidateMesh();
		bool hasValidBounds() const { return bounds_valid; }
	};
}
