#pragma once

#include <danikk_framework/octree.h>
#include <danikk_space_engine/config.h>
#include <danikk_space_engine/id.h>
#include <danikk_space_engine/mesh_render_data.h>

namespace danikk_space_engine
{
	struct PlanetSettings;

	/** @brief Stable per-frame reference to one terrain mesh and its world transform. */
	struct PlanetTerrainRenderItem
	{
		/** @brief Node ID used to resolve the mesh without retaining an invalidatable pointer. */
		PlanetTerrainNodeId node_id = static_cast<PlanetTerrainNodeId>(-1);
		/** @brief World-space origin of the node mesh. */
		glm::dvec3 world_position = glm::dvec3(0.0);
		/** @brief Rotation from the planet's local terrain frame into world space. */
		glm::dquat world_rotation = glm::dquat(1.0, 0.0, 0.0, 0.0);
	};

	/** @brief One adaptive spatial cell in a planet's render/streaming octree. */
	struct PlanetTerrainNode : public danikk_framework::OctreeNode<PlanetTerrainNodeId>
	{
		/** @brief Octree subdivision depth: zero at the coarse root, increasing toward detail. */
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
		/** @brief Exponent used to derive this planet's power-of-two root side length. */
		uint32 root_side_exponent = 0;
		/** @brief Half the per-planet root side length, including terrain height margin. */
		double initial_root_half_extent = 0.0;
		/** @brief Minimum render-node side length, derived from the 2^5 metre resolution. */
		double minimum_node_width = 0.0;
		/** @brief Number of subdivisions from the per-planet root to a minimum-width node. */
		uint8 maximum_depth = 0;
		static constexpr size_t maximum_node_count = 131072;
		/** @brief Global target-node budget; unlike drawing, this is not capped per depth. */
		static constexpr size_t maximum_target_nodes = 5120;
		uint64 next_node_generation_id = 1;
	private:
		size_t countTargetNodesRecursive(const glm::dvec3& center, double half_extent,
			uint8 depth, const glm::dvec3& camera_position, double height,
			const PlanetSettings& settings, double detail_scale,
			const glm::dvec3& render_window_minimum,
			const glm::dvec3& render_window_maximum, size_t& total) const;
		void refineNode(PlanetTerrainNodeId node_id, uint8 depth,
			const glm::dvec3& camera_position, double height,
			const PlanetSettings& settings, double detail_scale,
			const glm::dvec3& render_window_minimum,
			const glm::dvec3& render_window_maximum);
	public:

		PlanetTerrainTree() = delete;
		/** @brief Derives the root size, minimum node width, and depth from planet settings. */
		explicit PlanetTerrainTree(const PlanetSettings& settings);

		/**
		 * @brief Creates the canonical root and assigns it a fresh generation ID.
		 * @details Called by PlanetRenderData::applyTerrainChangesToTree() and the mesh scheduler when the tree is empty.
		 */
		PlanetTerrainNodeId createRoot();
		/**
		 * @brief Splits a leaf and assigns fresh generation IDs to its children.
		 * @details Called by refine() and mesh-job traversal to create the path to requested terrain cells.
		 */
		bool ensureChildren(PlanetTerrainNodeId parent_id);
		/**
		 * @brief Computes the target node half extent at a camera distance.
		 * @details Used by streaming refinement, render-list construction, and mesh scheduling to agree on LOD.
		 */
		double getTargetHalfExtent(const glm::dvec3& camera_position,
			double detail_scale, const glm::dvec3& center) const;
		/** @brief Selects a stable mesh mode from the node's position relative to the surface shell. */
		bool isSurfaceOnly(const PlanetTerrainNode& node, double planet_radius) const;
		/** @brief Tests membership in the fixed per-level render cube with its coarser-level center cutout. */
		bool isInRenderCoverage(const PlanetTerrainNode& node,
			const glm::dvec3& anchor, uint8 target_depth) const;
		/**
		 * @brief Computes the terrain-tree query window for a camera and planet height.
		 * @details Called by calculateStreamingDetailScale() before it counts target nodes.
		 */
		void updateRenderWindow(const glm::dvec3& camera_position, double height,
			const PlanetSettings& settings, glm::dvec3& render_window_minimum,
			glm::dvec3& render_window_maximum);
		/**
		 * @brief Computes streaming detail, render window, and visible-node budget diagnostics.
		 * @details Called by PlanetRenderData::updateTerrainStreaming() each frame before tree refinement.
		 */
		void calculateStreamingDetailScale(const glm::dvec3& camera_position, double height,
			const PlanetSettings& settings, glm::dvec3& render_window_minimum,
			glm::dvec3& render_window_maximum, double& detail_scale,
			size_t& target_node_count, size_t& detail_scale_adjustments);
		/**
		 * @brief Subdivides relevant leaves until they reach the requested detail scale.
		 * @details Called by PlanetRenderData::updateTerrainStreaming() to ensure target nodes exist for rendering and jobs.
		 */
		void refine(const glm::dvec3& camera_position, double height,
			const PlanetSettings& settings, double detail_scale,
			const glm::dvec3& render_window_minimum,
			const glm::dvec3& render_window_maximum);
	};

	/** @brief Spherical terrain region whose cached meshes must be regenerated. */
	struct PlanetTerrainDirtyRegion
	{
		glm::dvec3 center = glm::dvec3(0.0);
		double radius = 0.0;
	};
}
