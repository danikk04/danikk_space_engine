#pragma once

#include <danikk_space_engine/default.h>
#include <danikk_space_engine/world_environment.h>
#include <danikk_space_engine/world_physics.h>

#include <danikk_space_engine/world/point.h>
#include <danikk_space_engine/component/point/camera.h>
#include <danikk_space_engine/world/face_ptr.h>
#include <danikk_space_engine/world/chunk.h>
#include <danikk_space_engine/world/chunk_loader.h>
#include <danikk_space_engine/component/point/local_player_controller.h>
#include <danikk_space_engine/world/data.h>
#include <danikk_framework/dynamic_array.h>

namespace danikk_space_engine
{

	/**
	 * @brief Stores the nearest face hit and geometric data returned by a raycast.
	 */
	struct RaycastHit
	{
		bool hit = false;
		float distance = 0.0f;
		glm::dvec3 position = glm::dvec3(0.0);
		vec3 normal = vec3(0.0f);
		FacePtr face;
		uint8 triangle_index = 0;
		PointId points[3]{};
	};

	/**
	 * @brief Coordinates world state, simulation updates, physics, and geometry queries.
	 */
	class World
	{
	public:
		WorldData data;
		WorldEnvironment environment;
		WorldPhysics physics;
		ChunkLoader chunk_loader;
		DynamicArray<PointId> pending_point_destruction_ids;
		PointComponentSlotId camera_component_slot_id = 0;
		PointComponentSlotId local_player_controller_component_slot_id = 0;
		uint64 chunk_loader_generation_id = 0;
		DynamicArray<ChunkGenerationRequest> chunk_generation_requests;
		bool chunk_generation_requests_dirty = false;

		/**
		 * @brief Advances the world simulation by one fixed update step.
		 */
		void tick();

		/**
		 * @brief Adds a point ID to the deferred-destruction queue if it is not already queued.
		 */
		void deferPointDestruction(PointId point_id);

		/**
		 * @brief Replaces pending generation work and removes requests made for stale component generations.
		 */
		void replaceChunkGenerationRequests(
			WorldBodyComponentSlotId component_slot_id,
			const DynamicArray<ChunkGenerationRequest>& requests);

		/**
		 * @brief Removes chunk generation request.
		 */
		void removeChunkGenerationRequest(WorldBodyComponentSlotId component_slot_id, const glm::ivec3& grid_coordinate);

		/**
		 * @brief Runs one application frame, including profiling, GUI update, and world rendering.
		 */
		void frame();

		/**
		 * @brief Initializes the current world instance and its starting scenario.
		 */
		void init();

		/**
		 * @brief Stops current world activity and releases its owned resources.
		 */
		void shutdown();

		/**
		 * @brief Rejects chunks using their bounding spheres, tests remaining triangles, and returns the nearest hit.
		 */
		RaycastHit raycast(const glm::dvec3& origin, const vec3& direction, float max_distance = 10000.0f) const;

		/**
		 * @brief Finds the nearest ray intersection with from camera.
		 */
		RaycastHit raycastFromCamera(float max_distance = 10000.0f) const;

		/**
		 * @brief Returns the active point-component slot that owns the camera.
		 */
		PointComponentSlot& getCameraSlot();

		/**
		 * @brief Returns the active point-component slot that owns the camera.
		 */
		const PointComponentSlot& getCameraSlot() const;

		/**
		 * @brief Returns the camera component stored in the camera slot.
		 */
		CameraComponent& getCameraComponent();

		/**
		 * @brief Returns the camera component stored in the camera slot.
		 */
		const CameraComponent& getCameraComponent() const;

		/**
		 * @brief Returns the active point that defines the camera’s local position.
		 */
		Point& getCameraPoint();

		/**
		 * @brief Returns the active point that defines the camera’s local position.
		 */
		const Point& getCameraPoint() const;

		/**
		 * @brief Transforms the camera point through its chunk and world body into world-space coordinates.
		 */
		glm::dvec3 getCameraWorldPosition() const;

		/**
		 * @brief Transforms the chunk origin through its owning world body into world-space coordinates.
		 */
		glm::dvec3 getChunkWorldPosition(ChunkId chunk_id) const;

		/**
		 * @brief Transforms a point’s chunk-local position through the chunk and body transforms.
		 */
		glm::dvec3 getPointWorldPosition(ChunkId chunk_id, PointId point_id) const;

		/**
		 * @brief Returns the controller component attached to the configured controller slot.
		 */
		LocalPlayerControllerComponent& getLocalPlayerControllerComponent();

		/**
		 * @brief Returns the controller component attached to the configured controller slot.
		 */
		const LocalPlayerControllerComponent& getLocalPlayerControllerComponent() const;

		/**
		 * @brief Deletes faces.
		 */
		void deleteFaces(const DynamicArray<FacePtr>& faces);
	};

	extern World world;
}
