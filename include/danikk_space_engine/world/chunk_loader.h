#pragma once

#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <thread>
#include <vector>

#include <danikk_space_engine/default.h>

namespace danikk_space_engine
{
	using ChunkId = uint32;
	using WorldBodyId = uint32;

	struct ChunkGenerationRequest
	{
		uint64 generation_id = 0;
		WorldBodyId world_body_id = 0;
		glm::ivec3 grid_coordinate = glm::ivec3(0);
		glm::dvec3 chunk_position = glm::dvec3(0.0);
		glm::dvec3 up = glm::dvec3(0.0, 0.0, 1.0);
		glm::dvec3 east = glm::dvec3(1.0, 0.0, 0.0);
		glm::dvec3 north = glm::dvec3(0.0, 1.0, 0.0);
		double x0 = 0.0;
		double y0 = 0.0;
		double width = 0.0;
	};

	struct GeneratedChunkData
	{
		uint64 generation_id = 0;
		WorldBodyId world_body_id = 0;
		glm::ivec3 grid_coordinate = glm::ivec3(0);
		glm::dvec3 chunk_position = glm::dvec3(0.0);
		std::vector<vec3> point_positions;
		std::vector<uint32> triangle_point_indices;
	};

	class ChunkLoader
	{
		std::mutex mutex;
		std::condition_variable order_available;
		std::vector<ChunkGenerationRequest> requests;
		std::vector<GeneratedChunkData> completed;
		std::thread worker;
		bool stopping = false;
		bool has_active_request = false;
		ChunkGenerationRequest active_request;
		uint64 current_generation_id = 0;

		void run();
		static GeneratedChunkData generate(const ChunkGenerationRequest& request);

	public:
		ChunkLoader() = default;
		ChunkLoader(const ChunkLoader&) = delete;
		ChunkLoader& operator=(const ChunkLoader&) = delete;
		~ChunkLoader();

		void start();
		void stop();
		void updatePendingRequests(uint64 generation_id, const std::vector<ChunkGenerationRequest>& new_requests);
		bool tryTakeCompleted(GeneratedChunkData& result);
	};
}
