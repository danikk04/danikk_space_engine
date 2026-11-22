#pragma once

#include <danikk_space_engine/systems/planet/data.h>
#include <danikk_space_engine/systems/telemetry/telemetry.h>

namespace danikk_space_engine::planet_terrain_tree_internal
{
	inline constexpr PlanetTerrainNodeId invalid_node = static_cast<PlanetTerrainNodeId>(-1);
	inline constexpr size_t maximum_visible_nodes = 2048;
	inline constexpr size_t maximum_job_candidates_per_frame = 32;
	inline constexpr size_t maximum_tree_nodes = 131072;

	struct UpdateContext
	{
		PlanetData& planet;
		PlanetTerrainTree& tree;
		const glm::dvec3& camera_position;
		double height;
		const PlanetSettings& settings;
		TelemetrySystem& telemetry;
		glm::dvec3 render_window_minimum = glm::dvec3(0.0);
		glm::dvec3 render_window_maximum = glm::dvec3(0.0);
		bool render_window_initialized = false;
		double detail_scale = 0.0;
		size_t target_node_count = 0;
		size_t detail_scale_adjustments = 0;
		DynamicArray<PlanetTerrainNodeId> mesh_job_candidate_node_ids;
	};

	double getTargetHalfExtent(const UpdateContext& context, const glm::dvec3& center);
	bool isNodeRelevant(const UpdateContext& context, const PlanetTerrainNode& node);
	bool isSurfaceNode(const UpdateContext& context, const PlanetTerrainNode& node);
	bool hasCurrentMesh(const UpdateContext& context, PlanetTerrainNodeId node_id);
	void initialize(UpdateContext& context);
	void calculateDetailBudget(UpdateContext& context);
	void refine(UpdateContext& context);
	void collectMeshJobCandidates(UpdateContext& context);
	void activateMeshes(UpdateContext& context);
	void recordDiagnostics(const UpdateContext& context);
	void evictCachedMeshes(UpdateContext& context);
	void destroyTree(PlanetData& planet, TelemetrySystem& telemetry,
		MeshDeletionReason reason);
}
