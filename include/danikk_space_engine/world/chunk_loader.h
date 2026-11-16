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
	 * @brief Generates CPU-side planet chunk data on one worker and delivers it to the main thread.
	 *
	 * Requests/results are synchronized by a mutex. Pending requests are replaceable
	 * snapshots; the completed queue is limited to two entries to bound memory and
	 * back-pressure the worker. GPU mesh creation and WorldData installation remain
	 * main-thread operations performed after tryTakeCompleted().
	 */
	class ChunkLoader
	{
		/** @brief Protects request, result, active-job, and generation state shared with the worker. */
		std::mutex mutex;
		/** @brief Wakes the worker when new generation work arrives or shutdown begins. */
		std::condition_variable order_available;
		/** @brief Blocks the worker when the bounded completed-result queue is full. */
		std::condition_variable result_space_available;
		/** @brief Pending jobs; updatePendingRequests replaces this queue with the latest desired work. */
		DynamicArray<ChunkGenerationRequest> requests;
		/** @brief Completed CPU-only mesh results waiting for main-thread acceptance. */
		DynamicArray<GeneratedChunkData> completed;
		/** @brief Worker thread executing generatePlanetChunk(). */
		std::thread worker;
		/** @brief Set under the mutex to wake and terminate run(). */
		bool stopping = false;
		/** @brief True while a request is being generated outside the mutex. */
		bool has_active_request = false;
		/** @brief Snapshot of the currently executing request, used to avoid queueing duplicates. */
		ChunkGenerationRequest active_request;
		/** @brief World-lifetime token; results from older tokens are discarded. */
		uint64 current_generation_id = 0;

		/**
		 * @brief Waits for jobs, generates CPU mesh data without holding the mutex, and queues valid results.
		 *
		 * The result queue is bounded to two entries. The worker blocks when full until the main thread
		 * consumes a result or shutdown is requested. Stop does not cancel an active
		 * generation computation; it prevents that result from being published.
		 */
		void run();
	public:
		ChunkLoader() = default;
		ChunkLoader(const ChunkLoader&) = delete;
		ChunkLoader& operator=(const ChunkLoader&) = delete;
		~ChunkLoader();

		/**
		 * @brief Starts the worker when no thread is joinable.
		 * Repeated calls while running have no effect; a stopped loader can be started again.
		 */
		void start();

		/**
		 * @brief Wakes and joins the worker, then clears queued requests and results.
		 *
		 * A generation already in progress completes before the thread exits; its result is not published.
		 */
		void stop();

		/**
		 * @brief Replaces the pending queue and filters stale, active, and already-completed jobs.
		 * @param generation_id World-lifetime token copied into the new request set.
		 * @param new_requests Latest desired jobs; requests with a different token are ignored.
		 */
		void updatePendingRequests(uint64 generation_id, const DynamicArray<ChunkGenerationRequest>& new_requests);

		/**
		 * @brief Pops the oldest completed result matching the current world generation token.
		 * @param result Receives the moved result when one is available.
		 * @return True when a current-generation result was transferred; stale
		 *         results are discarded while searching, and false means none remain.
		 */
		bool tryTakeCompleted(GeneratedChunkData& result);
	};
}
