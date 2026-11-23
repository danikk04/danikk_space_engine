#pragma once

#include <danikk_framework/octree.h>
#include <danikk_space_engine/config.h>
#include <danikk_space_engine/id.h>
#include <danikk_space_engine/mesh_render_data.h>

namespace danikk_space_engine
{
	/** @brief One adaptive spatial cell in a planet's render/streaming octree. */
	struct PlanetTerrainNode : public danikk_framework::OctreeNode<PlanetTerrainNodeId>
	{
		MeshRenderData mesh;
		glm::dvec3 mesh_position = glm::dvec3(0.0);
		uint64 generation_id = 1;
		uint64 mesh_revision = 0;
		bool mesh_generated = false;
		PlanetMeshGenerator mesh_generator = PlanetMeshGenerator::marching_tetrahedra;
		bool mesh_surface_only = true;
		bool generation_pending = false;
		uint64 pending_terrain_revision = 0;
		uint64 pending_node_generation_id = 0;
		PlanetMeshGenerator pending_mesh_generator = PlanetMeshGenerator::marching_tetrahedra;
		bool pending_surface_only = true;
		bool render_active = false;

		/** @brief Tests this node's cubic bounds against an axis-aligned box. */
		bool intersectsBounds(const glm::dvec3& minimum, const glm::dvec3& maximum) const;
		/** @brief Tests this node's cubic bounds against a sphere. */
		bool intersectsSphere(const glm::dvec3& sphere_center, double sphere_radius) const;
		/** @brief Tests whether this node overlaps the configured surface shell. */
		bool intersectsSurfaceBand(double planet_radius, double below_surface,
			double above_surface) const;
		/** @brief Checks whether the stored mesh matches the requested terrain version and mode. */
		bool hasCurrentMesh(uint64 terrain_revision, PlanetMeshGenerator expected_generator,
			bool expected_surface_only) const;
	};

	/** @brief Adaptive spatial partition and render-node pool for a planet. */
	struct PlanetTerrainTree : public danikk_framework::Octree<PlanetTerrainNode, PlanetTerrainNodeId>
	{
		static constexpr PlanetTerrainNodeId invalid_node = static_cast<PlanetTerrainNodeId>(-1);
		/** @brief Half the root side length; root width is exactly 2^24 metres. */
		static constexpr double initial_root_half_extent = 8388608.0;
		/** @brief Smallest render-node side length, matching the former 32 m chunks. */
		static constexpr double minimum_node_width = 32.0;
		/** @brief Number of halvings from a 2^24 m root to a 32 m leaf. */
		static constexpr uint8 maximum_depth = 19;
		static constexpr size_t maximum_node_count = 131072;
		uint64 next_node_generation_id = 1;

		/** @brief Creates the canonical root and assigns it a fresh generation ID. */
		PlanetTerrainNodeId createRoot();
		/** @brief Splits a leaf and assigns fresh generation IDs to its children. */
		bool ensureChildren(PlanetTerrainNodeId parent_id);
	};

	/** @brief Spherical terrain region whose cached meshes must be regenerated. */
	struct PlanetTerrainDirtyRegion
	{
		glm::dvec3 center = glm::dvec3(0.0);
		double radius = 0.0;
	};
}
