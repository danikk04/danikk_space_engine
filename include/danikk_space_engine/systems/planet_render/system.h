#pragma once

#include <danikk_framework/dynamic_array.h>
#include <danikk_space_engine/systems/mesh_generation/mesh_generation.h>
#include <danikk_space_engine/systems/planet_render/data.h>

namespace danikk_space_engine
{
	enum class MeshDeletionReason : uint8;

	/** @brief Owns lazily allocated terrain render runtimes and draws active planet meshes. */
	class PlanetRenderSystem
	{
		/** @brief World providing canonical planets, camera state, generation, and telemetry. */
		World& world;
		/** @brief Runtime records exist only while a planet is relevant to the current view. */
		DynamicArray<PlanetRenderData> planet_render_data;
		/** @brief Monotonic token preventing stale jobs from targeting a re-created runtime. */
		uint64 next_runtime_generation_id = 1;

		/** @brief Finds the active terrain runtime for a canonical planet ID. */
		PlanetRenderData* findPlanetRenderData(PlanetId planet_id);
		/** @brief Finds the active terrain runtime without allowing mutation. */
		const PlanetRenderData* findPlanetRenderData(PlanetId planet_id) const;
		/** @brief Tests whether a planet is close or large enough in the view to need terrain meshes. */
		bool isPlanetRenderRelevant(const PlanetData& planet, const glm::dvec3& camera_position,
			const glm::dvec3& camera_front, double vertical_fov_degrees,
			double& surface_height) const;
		/** @brief Creates a runtime record or returns the existing record for a planet. */
		PlanetRenderData& ensurePlanetRenderData(const PlanetData& planet);
		/** @brief Updates one active planet's terrain streaming and mesh cache. */
		void updatePlanet(const PlanetData& planet, PlanetRenderData& render_data,
			double surface_height);
		/** @brief Cancels mesh jobs and releases the runtime record at the given array index. */
		void removePlanetRenderData(size_t index, MeshDeletionReason reason);
		/** @brief Selects the nearest missing mesh in one staged cube around the camera anchor. */
		bool findNextMeshGenerationJobInWindow(const PlanetData& planet,
			PlanetRenderData& render_data, const glm::dvec3& camera_world_position,
			uint8 window_radius, MeshGenerationJob& job,
			double& distance_squared, bool& window_covered);
		/** @brief Evicts cached meshes beyond the per-depth cache capacity. */
		void evictCachedMeshes(PlanetRenderData& render_data,
			const glm::dvec3& camera_position);
		/** @brief Draws active terrain meshes with the supplied camera matrices. */
		void drawTerrainMeshes(const glm::dmat4& view, const glm::dmat4& projection,
			const glm::dvec3& camera_position);

	public:
		/** @brief Binds this renderer to its owning world. */
		explicit PlanetRenderSystem(World& world) : world(world) {}
		/** @brief Releases runtime trees and GPU meshes while the graphics context is still alive. */
		void shutdown();
		/** @brief Updates relevance, lazily creates runtimes, and streams active planet terrain. */
		void update();
		/** @brief Removes one planet's cached render runtime before canonical planet data is deleted. */
		void destroyPlanet(PlanetId planet_id);
		/** @brief Adds an edit region to an existing runtime for precise mesh invalidation. */
		void markTerrainDirty(PlanetId planet_id, const PlanetTerrainDirtyRegion& region);
		/** @brief Finds the next missing terrain mesh for an available mesh-generation slot. */
		bool findNextMeshGenerationJob(MeshGenerationJob& job, double& distance_squared);
		/** @brief Validates a queued terrain job against planet and render-runtime generations. */
		bool isMeshGenerationJobWanted(const MeshGenerationJob& job) const;
		/** @brief Updates a runtime node's pending job identity. */
		void setMeshGenerationJobPending(const MeshGenerationJob& job, bool pending);
		/** @brief Installs a completed CPU mesh into the matching active terrain runtime. */
		void acceptGeneratedTerrainNodeMesh(const MeshGenerationJob& job);
		/** @brief Draws active planet terrain, using separate far and near depth ranges when needed. */
		void renderPlanetTerrainMeshes();
	};
}
