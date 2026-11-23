#pragma once

#include <danikk_space_engine/systems/planet/data.h>
#include <danikk_space_engine/systems/mesh_generation/mesh_generation.h>

namespace danikk_space_engine
{
	/** @brief Owns planet state and provides terrain queries, editing, streaming, and mesh acceptance. */
	class PlanetSystem
	{
		World& world;
		DynamicArray<PlanetData> planets;
		/** @brief Reused per-frame list of selected detail nodes and drawable fallbacks. */
		DynamicArray<PlanetTerrainRenderNode> terrain_render_nodes;
		/** @brief Reused scratch IDs for cache eviction candidates. */
		DynamicArray<PlanetTerrainNodeId> mesh_eviction_candidates;
		uint64 next_generation_id = 1;

		void updatePlanet(PlanetData& planet);
		void evictCachedMeshes(PlanetTerrainTree& tree, const glm::dvec3& camera_position);
		void cancelQueuedJobsForPlanet(const PlanetData& planet);
		bool findNextMeshGenerationJobInWindow(PlanetData& planet,
			const glm::dvec3& camera_world_position, uint8 window_radius,
			MeshGenerationJob& job, double& distance_squared, bool& window_covered);

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
		PlanetData* findPlanetByWorldBodyId(WorldBodyId world_body_id);
		const PlanetData* findPlanetByWorldBodyId(WorldBodyId world_body_id) const;
		bool isPlanetWorldBody(WorldBodyId world_body_id) const;
		void destroyPlanetForWorldBody(WorldBodyId world_body_id);
		PlanetData& getPlanet(PlanetId planet_id);
		const PlanetData& getPlanet(PlanetId planet_id) const;
		const DynamicArray<PlanetData>& getPlanets() const { return planets; }

		/** @brief Updates adaptive terrain-tree streaming once for the current world frame. */
		void update();
		/** @brief Finds the next job in the staged 9x9x9 detail-level coverage window. */
		bool findNextMeshGenerationJob(MeshGenerationJob& job, double& distance_squared);
		/** @brief Checks whether a queued planet mesh job is still relevant to the current frame. */
		bool isMeshGenerationJobWanted(const MeshGenerationJob& job) const;
		/** @brief Updates the node's pending flag after a job is assigned or canceled. */
		void setMeshGenerationJobPending(const MeshGenerationJob& job, bool pending);
		/** @brief Replaces a node's mesh with an accepted terrain generation result. */
		void acceptGeneratedTerrainNodeMesh(const MeshGenerationJob& job);
		/** @brief Fills and returns the current frame's selected terrain-node list. */
		const DynamicArray<PlanetTerrainRenderNode>& getTerrainNodesForRender();
		/** @brief Draws the supplied terrain-node list in the current depth pass. */
		void renderTerrainNodes(const DynamicArray<PlanetTerrainRenderNode>& render_nodes);
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
