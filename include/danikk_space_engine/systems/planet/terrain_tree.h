#pragma once

#include <danikk_framework/octree.h>
#include <danikk_space_engine/config.h>
#include <danikk_space_engine/id.h>
#include <danikk_space_engine/mesh_render_data.h>

namespace danikk_space_engine
{
	struct PlanetSettings;

	/** @brief One adaptive spatial cell in a planet's render/streaming octree. */
	struct PlanetTerrainNode : public danikk_framework::OctreeNode<PlanetTerrainNodeId>
	{
		/** @brief Parent node, or invalid_node when this node is the root. */
		PlanetTerrainNodeId parent_id = static_cast<PlanetTerrainNodeId>(-1);
		/** @brief Number of parent links from the tree root. */
		uint8 depth = 0;
		/** @brief GPU mesh generated for this node's terrain region, if available. */
		MeshRenderData mesh;
		/** @brief Planet-local origin subtracted from the mesh vertices for precision. */
		glm::dvec3 mesh_position = glm::dvec3(0.0);
		/** @brief Incarnation token used to reject jobs submitted for an older node state. */
		uint64 generation_id = 1;
		/** @brief Terrain revision represented by the currently stored mesh. */
		uint64 mesh_revision = 0;
		/** @brief Whether mesh generation completed for this node, including empty results. */
		bool mesh_generated = false;
		/** @brief Generator algorithm used to create the stored mesh. */
		PlanetMeshGenerator mesh_generator = PlanetMeshGenerator::marching_tetrahedra;
		/** @brief Whether the stored mesh contains only the planet's surface shell. */
		bool mesh_surface_only = true;
		/** @brief Whether a mesh generation job is queued, processing, or awaiting completion. */
		bool generation_pending = false;
		/** @brief Terrain revision captured by the pending generation job. */
		uint64 pending_terrain_revision = 0;
		/** @brief Node incarnation captured by the pending generation job. */
		uint64 pending_node_generation_id = 0;
		/** @brief Generator algorithm requested by the pending generation job. */
		PlanetMeshGenerator pending_mesh_generator = PlanetMeshGenerator::marching_tetrahedra;
		/** @brief Whether the pending job generates only the planet's surface shell. */
		bool pending_surface_only = true;
		/** @brief Whether this node's mesh is selected for drawing or fallback coverage. */
		bool render_active = false;
		/** @brief Temporary per-frame marker preventing duplicate fallback entries. */
		bool render_fallback = false;

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
		/** @brief Tests the render window, surface/cave range, and horizon for this node. */
		bool isInTerrainRenderRegion(const glm::dvec3& render_window_minimum,
			const glm::dvec3& render_window_maximum, const glm::dvec3& camera_position,
			double height, const PlanetSettings& settings) const;
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
		static constexpr size_t maximum_visible_meshes_per_level = 256;
		static constexpr size_t maximum_visible_nodes = maximum_visible_meshes_per_level
			* (maximum_depth + 1);
		uint64 next_node_generation_id = 1;

		/** @brief Creates the canonical root and assigns it a fresh generation ID. */
		PlanetTerrainNodeId createRoot();
		/** @brief Splits a leaf and assigns fresh generation IDs to its children. */
		bool ensureChildren(PlanetTerrainNodeId parent_id);
		/** @brief Computes the target node half extent at a camera distance. */
		static double getTargetHalfExtent(const glm::dvec3& camera_position,
			double detail_scale, const glm::dvec3& center);
		/** @brief Computes the terrain-tree query window for a camera and planet height. */
		static void updateRenderWindow(const glm::dvec3& camera_position, double height,
			const PlanetSettings& settings, glm::dvec3& render_window_minimum,
			glm::dvec3& render_window_maximum);
		/** @brief Computes streaming detail, render window, and visible-node budget diagnostics. */
		static void calculateStreamingDetailScale(const glm::dvec3& camera_position, double height,
			const PlanetSettings& settings, glm::dvec3& render_window_minimum,
			glm::dvec3& render_window_maximum, double& detail_scale,
			size_t& target_node_count, size_t& detail_scale_adjustments);
		/** @brief Subdivides relevant leaves until they reach the requested detail scale. */
		void refine(const glm::dvec3& camera_position, double height,
			const PlanetSettings& settings, double detail_scale,
			const glm::dvec3& render_window_minimum,
			const glm::dvec3& render_window_maximum);
	};

	/** @brief One node selected for this frame's terrain render list. */
	struct PlanetTerrainRenderNode
	{
		/** @brief Planet that owns the selected octree node. */
		PlanetId planet_id = static_cast<PlanetId>(-1);
		/** @brief Index of the selected node in its planet's terrain tree. */
		PlanetTerrainNodeId node_id = PlanetTerrainTree::invalid_node;
		/** @brief True when this node is an ancestor mesh filling a missing target. */
		bool fallback = false;
	};

	/** @brief Spherical terrain region whose cached meshes must be regenerated. */
	struct PlanetTerrainDirtyRegion
	{
		glm::dvec3 center = glm::dvec3(0.0);
		double radius = 0.0;
	};
}
