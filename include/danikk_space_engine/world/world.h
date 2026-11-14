#pragma once

#include <danikk_space_engine/default.h>

#include <danikk_space_engine/world/point.h>
#include <danikk_space_engine/component/point/camera.h>
#include <danikk_space_engine/world/face_ptr.h>
#include <danikk_space_engine/world/chunk.h>
#include <danikk_space_engine/world/chunk_loader.h>
#include <danikk_space_engine/component/point/local_player_controller.h>
#include <danikk_space_engine/world/world_data.h>
#include <danikk_framework/dynamic_array.h>

namespace danikk_space_engine
{
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

	struct PlanetChunkRecord
	{
		glm::ivec3 grid_coordinate = glm::ivec3(0);
		ChunkId chunk_id = 0;
	};

	class World
	{
	public:
		WorldData data;
		ChunkLoader chunk_loader;
		DynamicArray<PointId> pending_point_destruction_ids;
		PointComponentSlotId camera_component_slot_id = 0;
		PointComponentSlotId local_player_controller_component_slot_id = 0;
		WorldBodyId planet_body_id = 0;
		DynamicArray<PlanetChunkRecord> planet_chunks;
		DynamicArray<glm::ivec3> desired_planet_chunk_keys;
		glm::dvec3 planet_stream_anchor = glm::dvec3(0.0);
		bool planet_stream_initialized = false;
		uint64 planet_generation_id = 0;

		void tick();
		void deferPointDestruction(PointId point_id);

		void frame();

		void init();
		void shutdown();

		RaycastHit raycast(const glm::dvec3& origin, const vec3& direction, float max_distance = 10000.0f) const;
		RaycastHit raycastFromCamera(float max_distance = 10000.0f) const;
		PointComponentSlot& getCameraSlot();
		const PointComponentSlot& getCameraSlot() const;
		CameraComponent& getCameraComponent();
		const CameraComponent& getCameraComponent() const;
		Point& getCameraPoint();
		const Point& getCameraPoint() const;
		glm::dvec3 getCameraWorldPosition() const;
		glm::dvec3 getChunkWorldPosition(ChunkId chunk_id) const;
		glm::dvec3 getPointWorldPosition(ChunkId chunk_id, PointId point_id) const;
		LocalPlayerControllerComponent& getLocalPlayerControllerComponent();
		const LocalPlayerControllerComponent& getLocalPlayerControllerComponent() const;
		DynamicArray<FacePtr> extrudeFaces(const DynamicArray<FacePtr>& faces, float distance);
		void deleteVolumeCells(const DynamicArray<FacePtr>& faces);
	};

	extern World world;
}
