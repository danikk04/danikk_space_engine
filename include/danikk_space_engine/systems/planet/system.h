#pragma once

#include <danikk_space_engine/systems/planet/data.h>
#include <danikk_space_engine/systems/mesh_generation/mesh_generation.h>
#include <danikk_space_engine/systems/telemetry/mesh_generation.h>

namespace danikk_space_engine
{
	/** @brief Owns planet state and provides terrain queries, editing, streaming, and mesh acceptance. */
	class PlanetSystem
	{
		static constexpr size_t maximum_visible_meshes_per_level = 256;
		static constexpr size_t maximum_visible_nodes = maximum_visible_meshes_per_level
			* (PlanetTerrainTree::maximum_depth + 1);

		World& world;
		DynamicArray<PlanetData> planets;
		uint64 next_generation_id = 1;

		void updatePlanet(PlanetData& planet);
		void updatePlanetTerrainTree(PlanetData& planet, const glm::dvec3& camera_position,
			double height);
		double getTargetHalfExtent(const glm::dvec3& camera_position, double detail_scale,
			const glm::dvec3& center) const;
		void updateRenderWindow(const glm::dvec3& camera_position, double height,
			const PlanetSettings& settings, glm::dvec3& render_window_minimum,
			glm::dvec3& render_window_maximum) const;
		bool isNodeRelevant(const glm::dvec3& render_window_minimum,
			const glm::dvec3& render_window_maximum, const glm::dvec3& camera_position,
			double height, const PlanetSettings& settings,
			const PlanetTerrainNode& node) const;
		void synchronizeTerrainTree(PlanetData& planet);
		void calculateDetailBudget(const glm::dvec3& camera_position, double height,
			const PlanetSettings& settings, glm::dvec3& render_window_minimum,
			glm::dvec3& render_window_maximum, double& detail_scale,
			size_t& target_node_count, size_t& detail_scale_adjustments) const;
		void refineTerrainTree(PlanetTerrainTree& tree, const glm::dvec3& camera_position,
			double height, const PlanetSettings& settings, double detail_scale,
			const glm::dvec3& render_window_minimum,
			const glm::dvec3& render_window_maximum) const;
		void updateRenderActivity(PlanetData& planet, const glm::dvec3& camera_position,
			double height, const PlanetSettings& settings, double detail_scale,
			const glm::dvec3& render_window_minimum,
			const glm::dvec3& render_window_maximum,
			DynamicArray<PlanetTerrainNodeId>& selected_node_ids);
		void activateMeshes(PlanetTerrainTree& tree,
			const DynamicArray<PlanetTerrainNodeId>& selected_node_ids) const;
		void recordDiagnostics(const PlanetData& planet, size_t target_node_count,
			size_t detail_scale_adjustments, double detail_scale);
		void evictCachedMeshes(PlanetTerrainTree& tree, const glm::dvec3& camera_position);
		void destroyPlanetTerrainTree(PlanetData& planet, MeshDeletionReason reason);
		void cancelQueuedJobsForPlanet(const PlanetData& planet);

	public:
		explicit PlanetSystem(World& world) : world(world) {}
		/** @brief Logs subsystem initialization. */
		void init();
		/** @brief Logs subsystem shutdown. */
		void shutdown();

		PlanetData& createPlanet(PlanetSettingsId settings_id,
			const glm::dvec3& position = glm::dvec3(0.0),
			const glm::dquat& rotation = glm::dquat(1.0, 0.0, 0.0, 0.0));
		void destroyPlanet(PlanetId planet_id);
		bool isPlanet(PlanetId planet_id) const;
		PlanetData* findPlanet(PlanetId planet_id);
		const PlanetData* findPlanet(PlanetId planet_id) const;
		PlanetData& getPlanet(PlanetId planet_id);
		const PlanetData& getPlanet(PlanetId planet_id) const;
		const DynamicArray<PlanetData>& getPlanets() const { return planets; }

		/** @brief Updates adaptive terrain-tree streaming once for the current world frame. */
		void update();
		/** @brief Finds the nearest eligible node on the least-queued relevant tree level. */
		bool findNextMeshGenerationJob(MeshGenerationJob& job, double& distance_squared) const;
		/** @brief Checks whether a queued planet mesh job is still relevant to the current frame. */
		bool isMeshGenerationJobWanted(const MeshGenerationJob& job) const;
		/** @brief Updates the node's pending flag after a job is assigned or canceled. */
		void setMeshGenerationJobPending(const MeshGenerationJob& job, bool pending);
		void acceptGeneratedSurfaceNode(const MeshGenerationJob& job);
		/** @brief Draws active adaptive surface-node meshes in the current depth pass. */
		void renderSurfaceNodes();
		/**
		 * @brief Applies and persists a spherical terrain edit on a registered planet.
		 * @param planet_id Planet to modify.
		 * @param position World-space center of the edit sphere.
		 * @param radius Edit sphere radius in metres; nonpositive values are rejected.
		 * @param material_id Fill material, or material ID zero to remove terrain.
		 * @return True when the planet was found and the edit was added.
		 */
		bool editPlanetGround(PlanetId planet_id, const glm::dvec3& position,
			double radius, MaterialId material_id);
		/** @brief Removes a terrain edit by its stable planet-local ID. */
		bool removePlanetTerrainEdit(PlanetId planet_id, PlanetTerrainEditId edit_id);
		/** @brief Returns immutable settings from the planet settings catalog. */
		const PlanetSettings& getPlanetSettings(PlanetSettingsId id) const;

		double getPlanetDensity(PlanetId planet_id, const glm::dvec3& position,
			PlanetTerrainMode mode) const;
		MaterialId getPlanetMaterial(PlanetId planet_id, const glm::dvec3& position,
			PlanetTerrainMode mode) const;
		double getPlanetHeightAboveSurface(PlanetId planet_id, const glm::dvec3& position) const;
		/** @brief Blends the nearest planet's atmosphere color into black space by altitude. */
		glm::vec3 getAtmosphereBackgroundColor(const glm::dvec3& world_position) const;
		/** @brief Samples the cave-free surface from current planet data, optionally omitting fine noise layers. */
		double getPlanetSurfaceRadius(PlanetId planet_id,
			const glm::dvec3& direction,
			PlanetTerrainMode mode, double minimum_wavelength = 0.0) const;
	};

}
