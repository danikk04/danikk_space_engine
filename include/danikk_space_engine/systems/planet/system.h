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
		uint64 next_generation_id = 1;

		/**
		 * @brief Updates one planet's camera-relative streaming state and cache.
		 * @details Called by update() once per registered planet to refresh terrain coverage.
		 */
		void updatePlanet(PlanetData& planet);
		/**
		 * @brief Removes cached meshes that are no longer useful to the current camera region.
		 * @details Called by updatePlanet() after streaming selects the current render coverage.
		 */
		void evictCachedMeshes(PlanetTerrainTree& tree, const glm::dvec3& camera_position);
		/**
		 * @brief Cancels queued mesh-generation work belonging to a planet.
		 * @details Called before planet destruction or system shutdown so stale jobs do not keep running.
		 */
		void cancelQueuedJobsForPlanet(const PlanetData& planet);
		/**
		 * @brief Finds the nearest missing mesh in one staged cube around the camera anchor.
		 * @details Called by findNextMeshGenerationJob() for each coverage radius and planet.
		 */
		bool findNextMeshGenerationJobInWindow(PlanetData& planet,
			const glm::dvec3& camera_world_position, uint8 window_radius,
			MeshGenerationJob& job, double& distance_squared, bool& window_covered);

	public:
		/**
		 * @brief Creates a planet subsystem bound to its owning world.
		 * @details WorldSystemContainer constructs it so subsystem operations can access sibling systems.
		 */
		explicit PlanetSystem(World& world) : world(world) {}
		/**
		 * @brief Logs initialization of the planet subsystem.
		 * @details Called by World::init() to record subsystem startup in the application log.
		 */
		void init();
		/**
		 * @brief Cancels planet jobs and releases cached terrain meshes.
		 * @details Called by World::shutdown() before the owning world is destroyed.
		 */
		void shutdown();

		/**
		 * @brief Registers a planet and loads its saved terrain edits.
		 * @details Called by scenario setup; assigns an ID and initializes planet-owned state.
		 */
		PlanetData& createPlanet(PlanetSettingsId settings_id,
			const glm::dvec3& position = glm::dvec3(0.0),
			const glm::dquat& rotation = glm::dquat(1.0, 0.0, 0.0, 0.0));
		/**
		 * @brief Removes a registered planet and releases its jobs and terrain meshes.
		 * @details Called by destroyPlanetForWorldBody() when the planet's owning body is removed.
		 */
		void destroyPlanet(PlanetId planet_id);
		/**
		 * @brief Checks whether a planet ID currently belongs to a registered planet.
		 * @details Called by terrain tools before applying edits to a selected planet.
		 */
		bool isPlanet(PlanetId planet_id) const;
		/**
		 * @brief Finds a registered planet by its stable planet ID.
		 * @details Used by mesh generation, save handling, and other planet APIs to resolve IDs safely.
		 */
		PlanetData* findPlanet(PlanetId planet_id);
		/**
		 * @brief Finds a registered planet by its stable planet ID without allowing mutation.
		 * @details Used by read-only generation and save paths that need planet data after ID validation.
		 */
		const PlanetData* findPlanet(PlanetId planet_id) const;
		/**
		 * @brief Finds the planet associated with a world-body ID.
		 * @details Called by raycast and human-ground systems to map body hits to planet terrain.
		 */
		PlanetData* findPlanetByWorldBodyId(WorldBodyId world_body_id);
		/**
		 * @brief Finds the planet associated with a world-body ID without allowing mutation.
		 * @details Called by read-only raycast and human-ground queries to resolve the terrain owner.
		 */
		const PlanetData* findPlanetByWorldBodyId(WorldBodyId world_body_id) const;
		/**
		 * @brief Checks whether a world-body ID identifies a planet body.
		 * @details Called by physics, controller, and tools to apply planet-specific body rules.
		 */
		bool isPlanetWorldBody(WorldBodyId world_body_id) const;
		/**
		 * @brief Removes the planet attached to a world body.
		 * @details Called by WorldBodySystem during body destruction to keep planet ownership synchronized.
		 */
		void destroyPlanetForWorldBody(WorldBodyId world_body_id);
		/**
		 * @brief Returns a registered planet or raises a fatal error for an invalid ID.
		 * @details Called by PhysicsSystem to resolve a target planet and read its transform.
		 */
		PlanetData& getPlanet(PlanetId planet_id);
		/**
		 * @brief Returns a registered planet as read-only data or raises a fatal error for an invalid ID.
		 * @details Called by read-only PhysicsSystem paths to resolve a target planet and inspect its transform.
		 */
		const PlanetData& getPlanet(PlanetId planet_id) const;
		/**
		 * @brief Returns all registered planets as read-only data.
		 * @details Iterated by world rendering, environment, raycast, and diagnostics systems.
		 */
		const DynamicArray<PlanetData>& getPlanets() const { return planets; }

		/**
		 * @brief Updates each planet's adaptive terrain streaming for the current frame.
		 * @details Called by World::frame() before mesh-job processing and world rendering, using the current camera state.
		 */
		void update();
		/**
		 * @brief Finds the next missing terrain mesh in the staged coverage windows.
		 * @details Called by MeshGenerationSystem when it needs another planet job for an available task slot.
		 */
		bool findNextMeshGenerationJob(MeshGenerationJob& job, double& distance_squared);
		/**
		 * @brief Checks whether a queued planet mesh job still matches current terrain and camera state.
		 * @details Called by MeshGenerationSystem before starting or continuing asynchronous generation.
		 */
		bool isMeshGenerationJobWanted(const MeshGenerationJob& job) const;
		/**
		 * @brief Marks a planet node's mesh job as pending or clears its matching pending state.
		 * @details Called by MeshGenerationSystem on job assignment, cancellation, and completion paths.
		 */
		void setMeshGenerationJobPending(const MeshGenerationJob& job, bool pending);
		/**
		 * @brief Accepts a completed terrain mesh and installs its GPU representation on the node.
		 * @details Called by MeshGenerationSystem on the main thread after generation finishes.
		 */
		void acceptGeneratedTerrainNodeMesh(const MeshGenerationJob& job);
		/**
		 * @brief Draws meshes from the per-planet terrain render lists.
		 * @details Called by World::renderWorldMeshes() in the dedicated planet-terrain render pass.
		 */
		void renderPlanetTerrainMeshes();
		/**
		 * @brief Applies and persists a spherical terrain edit on a registered planet.
		 * @details Called by the terrain tool after its raycast identifies the target planet and position.
		 * @param planet_id Planet to modify.
		 * @param position World-space center of the edit sphere.
		 * @param radius Edit sphere radius in metres; nonpositive values are rejected.
		 * @param material_id Fill material, or material ID zero to remove terrain.
		 * @return True when the planet was found and the edit was added.
		 */
		bool editPlanetGround(PlanetId planet_id, const glm::dvec3& position,
			double radius, MaterialId material_id);
		/**
		 * @brief Removes a terrain edit by its stable planet-local ID.
		 * @details No current production call site; retained for edit undo/removal workflows.
		 */
		bool removePlanetTerrainEdit(PlanetId planet_id, PlanetTerrainEditId edit_id);
		/**
		 * @brief Returns immutable settings from the planet settings catalog.
		 * @details Used by terrain generation, rendering, raycast, environment, and scenario code.
		 */
		const PlanetSettings& getPlanetSettings(PlanetSettingsId id) const;

		/**
		 * @brief Samples signed terrain density using the requested cave/edit mode.
		 * @details Called by terrain mesh generation, raycast, physics, human grounding, and scenarios.
		 */
		double getPlanetDensity(PlanetId planet_id, const glm::dvec3& position,
			PlanetTerrainMode mode) const;
		/**
		 * @brief Samples the terrain material using the requested cave/edit mode.
		 * @details Called by planet mesh generation to assign a material to each generated triangle.
		 */
		MaterialId getPlanetMaterial(PlanetId planet_id, const glm::dvec3& position,
			PlanetTerrainMode mode) const;
		/**
		 * @brief Computes signed height relative to the edited planet surface.
		 * @details Called by World camera updates and debug display to classify altitude and underground state.
		 */
		double getPlanetHeightAboveSurface(PlanetId planet_id, const glm::dvec3& position) const;
		/**
		 * @brief Blends the nearest planet's atmosphere color into black space by altitude.
		 * @details Called by World rendering to select the background color for the current camera position.
		 */
		glm::vec3 getAtmosphereBackgroundColor(const glm::dvec3& world_position) const;
		/**
		 * @brief Samples the edited planet's radial surface distance, optionally omitting fine noise layers.
		 * @details Called by scenario setup to place the initial camera and other surface-dependent objects.
		 */
		double getPlanetSurfaceRadius(PlanetId planet_id,
			const glm::dvec3& direction,
			PlanetTerrainMode mode, double minimum_wavelength = 0.0) const;
	};

}
