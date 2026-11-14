#pragma once

#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <thread>

#include <danikk_framework/dynamic_array.h>
#include <danikk_space_engine/default.h>
#include <danikk_space_engine/component/world_body/planet/chunk_generator.h>

namespace danikk_space_engine
{
	class ChunkLoader
	{
		std::mutex mutex;
		std::condition_variable order_available;
		DynamicArray<ChunkGenerationRequest> requests;
		DynamicArray<GeneratedChunkData> completed;
		std::thread worker;
		bool stopping = false;
		bool has_active_request = false;
		ChunkGenerationRequest active_request;
		uint64 current_generation_id = 0;

		void run();
	public:
		ChunkLoader() = default;
		ChunkLoader(const ChunkLoader&) = delete;
		ChunkLoader& operator=(const ChunkLoader&) = delete;
		~ChunkLoader();

		void start();
		void stop();
		void updatePendingRequests(uint64 generation_id, const DynamicArray<ChunkGenerationRequest>& new_requests);
		bool tryTakeCompleted(GeneratedChunkData& result);
	};
}
