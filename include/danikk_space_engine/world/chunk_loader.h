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

	/**
	 * @brief Generates chunks on a worker thread and delivers results to the main thread.
	 */
	class ChunkLoader
	{
		std::mutex mutex;
		std::condition_variable order_available;
		std::condition_variable result_space_available;
		DynamicArray<ChunkGenerationRequest> requests;
		DynamicArray<GeneratedChunkData> completed;
		std::thread worker;
		bool stopping = false;
		bool has_active_request = false;
		ChunkGenerationRequest active_request;
		uint64 current_generation_id = 0;

		/**
		 * @brief Performs the run operation.
		 */
		void run();
	public:
		ChunkLoader() = default;
		ChunkLoader(const ChunkLoader&) = delete;
		ChunkLoader& operator=(const ChunkLoader&) = delete;
		~ChunkLoader();

		/**
		 * @brief Performs the start operation.
		 */
		void start();

		/**
		 * @brief Performs the stop operation.
		 */
		void stop();

		/**
		 * @brief Updates pending requests.
		 */
		void updatePendingRequests(uint64 generation_id, const DynamicArray<ChunkGenerationRequest>& new_requests);

		/**
		 * @brief Attempts to obtain take completed.
		 */
		bool tryTakeCompleted(GeneratedChunkData& result);
	};
}
