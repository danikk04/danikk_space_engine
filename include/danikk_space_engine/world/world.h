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
		 * @brief Performs the tick operation.
		 */
		void tick();

		/**
		 * @brief Performs the defer point destruction operation.
		 */
		void deferPointDestruction(PointId point_id);

		/**
		 * @brief Performs the replace chunk generation requests operation.
		 */
		void replaceChunkGenerationRequests(
			WorldBodyComponentSlotId component_slot_id,
			const DynamicArray<ChunkGenerationRequest>& requests);

		/**
		 * @brief Removes chunk generation request.
		 */
		void removeChunkGenerationRequest(WorldBodyComponentSlotId component_slot_id, const glm::ivec3& grid_coordinate);

		/**
		 * @brief Performs the frame operation.
		 */
		void frame();

		/**
		 * @brief Performs the init operation.
		 */
		void init();

		/**
		 * @brief Performs the shutdown operation.
		 */
		void shutdown();

		/**
		 * @brief Performs the raycast operation.
		 */
		RaycastHit raycast(const glm::dvec3& origin, const vec3& direction, float max_distance = 10000.0f) const;

		/**
		 * @brief Finds the nearest ray intersection with from camera.
		 */
		RaycastHit raycastFromCamera(float max_distance = 10000.0f) const;

		/**
		 * @brief Returns camera slot.
		 */
		PointComponentSlot& getCameraSlot();

		/**
		 * @brief Returns camera slot.
		 */
		const PointComponentSlot& getCameraSlot() const;

		/**
		 * @brief Returns camera component.
		 */
		CameraComponent& getCameraComponent();

		/**
		 * @brief Returns camera component.
		 */
		const CameraComponent& getCameraComponent() const;

		/**
		 * @brief Returns camera point.
		 */
		Point& getCameraPoint();

		/**
		 * @brief Returns camera point.
		 */
		const Point& getCameraPoint() const;

		/**
		 * @brief Returns camera world position.
		 */
		glm::dvec3 getCameraWorldPosition() const;

		/**
		 * @brief Returns chunk world position.
		 */
		glm::dvec3 getChunkWorldPosition(ChunkId chunk_id) const;

		/**
		 * @brief Returns point world position.
		 */
		glm::dvec3 getPointWorldPosition(ChunkId chunk_id, PointId point_id) const;

		/**
		 * @brief Returns local player controller component.
		 */
		LocalPlayerControllerComponent& getLocalPlayerControllerComponent();

		/**
		 * @brief Returns local player controller component.
		 */
		const LocalPlayerControllerComponent& getLocalPlayerControllerComponent() const;

		/**
		 * @brief Deletes faces.
		 */
		void deleteFaces(const DynamicArray<FacePtr>& faces);
	};

	extern World world;
}
