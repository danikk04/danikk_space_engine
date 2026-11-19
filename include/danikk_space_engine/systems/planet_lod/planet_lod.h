#pragma once

#include <memory>

#include <danikk_framework/dynamic_array.h>
#include <danikk_engine/mesh.h>
#include <danikk_space_engine/fwd.h>
#include <danikk_space_engine/id.h>

namespace danikk_space_engine
{
	inline constexpr uint8 planet_lod_level_count = 6;
	inline constexpr uint8 planet_lod_face_count = 6;

	/** @brief Material range in one cube-sphere face mesh. */
	struct PlanetLodMaterialRange
	{
		/** @brief Material drawn for this contiguous index range. */
		MaterialId material_id = 0;
		/** @brief First index in the combined planetary mesh. */
		uint32 first_index = 0;
		/** @brief Number of indices in the range. */
		uint32 index_count = 0;
	};

	/** @brief One generated face or camera-local surface tile belonging to a LOD level. */
	struct PlanetLodPatch
	{
		/** @brief Mesh vertices relative to `origin`, in meters. */
		DynamicArray<vec3> positions;
		/** @brief Triangle indices referencing `positions`. */
		DynamicArray<uint32> triangle_indices;
		/** @brief One material ID per triangle. */
		DynamicArray<MaterialId> triangle_material_ids;
		/** @brief Origin of this tile relative to the planet center. */
		glm::dvec3 origin = glm::dvec3(0.0);
		/** @brief Tile key used to reject results for a previous camera location. */
		glm::ivec3 grid_coordinate = glm::ivec3(0);
		/** @brief True after the worker result for this tile was accepted. */
		bool ready = false;
	};

	/** @brief Cached camera-local tile or full-planet surface at one sampling resolution. */
	struct PlanetLodLevel
	{
		/** @brief One GPU mesh combining this level's generated tiles. */
		Mesh* mesh = nullptr;
		/** @brief Material draw ranges in `mesh`. */
		DynamicArray<PlanetLodMaterialRange> material_ranges;
		/** @brief Worker payloads for this level's tiles. */
		DynamicArray<PlanetLodPatch> patches;
		/** @brief Grid resolution along one tile edge. */
		uint32 resolution = 0;
		/** @brief True when this level is a high-detail tile anchored near the camera. */
		bool camera_local = false;
		/** @brief Origin of the currently uploaded local tile mesh. */
		glm::dvec3 mesh_origin = glm::dvec3(0.0);
		/** @brief True once all faces were combined into `mesh`. */
		bool initialized = false;
	};

	/** @brief Per-planet LOD selection, hysteresis, and cached surface levels. */
	struct PlanetLodPlanet
	{
		/** @brief Planet's owning body. */
		WorldBodyId world_body_id = static_cast<WorldBodyId>(-1);
		/** @brief Prevents results from a destroyed/recreated planet from being accepted. */
		uint64 planet_generation_id = 0;
		/** @brief Rejects LOD payloads generated before a terrain edit. */
		uint64 terrain_revision = 0;
		/** @brief Desired level selected by camera altitude and hysteresis. */
		uint8 desired_level = 0;
		/** @brief Last complete level available for drawing. */
		uint8 active_level = 0;
		/** @brief Whether a desired level has been selected before. */
		bool has_desired_level = false;
		/** @brief Whether any complete level is available for drawing. */
		bool has_active_level = false;
		/** @brief Whether the camera is above this planet's surface. */
		bool enabled = false;
		/** @brief Whether the high-detail local tile should be drawn this frame. */
		bool show_local_patch = false;
		/** @brief Immutable field state used by background face generation. */
		std::shared_ptr<const PlanetTerrainSnapshot> terrain_snapshot;
		/** @brief Nested storage: one tile-payload array for each LOD level. */
		DynamicArray<PlanetLodLevel> levels;
	};

	/**
	 * @brief Selects and renders a detailed local surface tile and five global surface levels.
	 *
	 * The first level follows the camera and uses meter-scale local UVs. Higher levels
	 * cover the complete planet using cube-sphere faces. CPU generation is delegated
	 * to ChunkMeshGenerationSystem; this system owns the uploaded meshes.
	 */
	class PlanetLodSystem
	{
		/** @brief World providing planets, camera transforms, and the shared mesh worker. */
		World& world;
		/** @brief Per-planet arrays of level data and one combined GPU mesh per level. */
		DynamicArray<PlanetLodPlanet> planets;

		PlanetLodPlanet& getOrCreatePlanet(const PlanetData& planet);
		void destroyLevelMeshes(PlanetLodPlanet& planet);

	public:
		explicit PlanetLodSystem(World& world) : world(world) {}
		PlanetLodSystem(const PlanetLodSystem&) = delete;
		PlanetLodSystem& operator=(const PlanetLodSystem&) = delete;

		/** @brief Chooses levels, queues missing tile meshes, and removes stale planet records. */
		void update();
		/** @brief Draws the selected global surface mesh for the far rendering range. */
		void drawFar() const;
		/** @brief Draws the camera-local detailed tile in the near rendering range. */
		void drawNear() const;
		/** @brief Accepts one asynchronous surface-tile result and uploads its GPU mesh. */
		void acceptGeneratedMesh(const GeneratedChunkMesh& generated_mesh);
		/** @brief Releases all LOD meshes while the graphics context is still active. */
		void clear();
	};
}
