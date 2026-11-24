#pragma once

#include <danikk_framework/dynamic_array.h>
#include <danikk_space_engine/systems/planet/data.h>
#include <danikk_space_engine/systems/planet_render/terrain_tree.h>

namespace danikk_space_engine
{
	class MeshGenerationTelemetry;
	enum class MeshDeletionReason : uint8;

	/** @brief Cached terrain rendering and streaming state for one currently relevant planet. */
	struct PlanetRenderData
	{
		/** @brief Canonical planet whose terrain runtime this record owns. */
		PlanetId planet_id;
		/** @brief Runtime incarnation token used to reject jobs after this cache is unloaded. */
		uint64 runtime_generation_id;
		/** @brief Terrain revision most recently synchronized into the cached octree. */
		uint64 terrain_tree_revision;
		/** @brief Adaptive terrain octree and its CPU/GPU mesh cache. */
		PlanetTerrainTree terrain_tree;
		/** @brief Meshes selected for the current frame, with their world transforms. */
		DynamicArray<PlanetTerrainRenderItem> terrain_render_items;
		/** @brief Edited regions awaiting propagation into the cached octree. */
		DynamicArray<PlanetTerrainDirtyRegion> pending_terrain_dirty_regions;
		/** @brief Latest camera position in the planet's local frame. */
		glm::dvec3 streaming_camera_position = glm::dvec3(0.0);
		/** @brief Minimum corner of the latest local AABB used to find relevant terrain nodes. */
		glm::dvec3 render_window_minimum = glm::dvec3(0.0);
		/** @brief Maximum corner of the latest local AABB used to find relevant terrain nodes. */
		glm::dvec3 render_window_maximum = glm::dvec3(0.0);
		/** @brief Latest camera height above the surface, negative when underground. */
		double streaming_height = 0.0;
		/** @brief Detail scale selected for the latest camera position. */
		double streaming_detail_scale = 0.0;
		/** @brief Whether this runtime was selected for the current frame's rendering/streaming. */
		bool active_this_frame = false;
		/** @brief Whether camera data has initialized streaming for this runtime. */
		bool terrain_streaming_active = false;

		/** @brief Creates a fresh terrain cache initialized to the planet's current terrain revision. */
		PlanetRenderData(const PlanetData& planet, const PlanetSettings& settings,
			uint64 runtime_generation_id);

		/** @brief Updates the tree, target coverage, render list, and diagnostics for this frame. */
		void updateTerrainStreaming(const PlanetData& planet, const glm::dvec3& camera_position,
			double height, const PlanetSettings& settings, MeshGenerationTelemetry& telemetry);
		/** @brief Builds the final render list from this runtime's tree and the planet transform. */
		void buildTerrainRenderList(const PlanetData& planet, const PlanetSettings& settings);
		/** @brief Computes the target octree depth at this runtime's camera anchor. */
		uint8 getCurrentTerrainDepth() const;
		/** @brief Applies pending edits and enforces the adaptive tree node limit. */
		void applyTerrainChangesToTree(const PlanetData& planet, MeshGenerationTelemetry& telemetry);
		/** @brief Records terrain tree and mesh-job diagnostics for this runtime. */
		void recordDiagnostics(MeshGenerationTelemetry& telemetry, size_t target_node_count,
			size_t detail_scale_adjustments, double detail_scale) const;
		/** @brief Releases every cached GPU mesh and clears this runtime's octree. */
		void destroyTerrainTree(MeshGenerationTelemetry& telemetry, MeshDeletionReason reason);
	};
}
