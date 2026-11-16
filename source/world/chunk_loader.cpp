#include <danikk_space_engine/world/chunk_loader.h>

namespace danikk_space_engine
{
	namespace
	{
		/** @brief Maximum number of generated chunk payloads buffered for the main thread. */
		constexpr size_t max_completed_chunks = 2;

		/**
		 * @brief Compares the full request identity while ignoring copied settings and output arrays.
		 * @param left First generation request.
		 * @param right Second generation request.
		 * @return True when world generation, body, component slot/incarnation,
		 *         terrain revision, and grid coordinate all match.
		 */
		bool sameRequestKey(const ChunkGenerationRequest& left, const ChunkGenerationRequest& right)
		{
			return left.generation_id == right.generation_id
				&& left.component_slot_id == right.component_slot_id
				&& left.component_generation_id == right.component_generation_id
				&& left.terrain_revision == right.terrain_revision
				&& left.world_body_id == right.world_body_id
				&& left.grid_coordinate == right.grid_coordinate;
		}

		/**
		 * @brief Prevents queueing work whose matching result is already awaiting the main thread.
		 * @param request Request identity to look for.
		 * @param completed Bounded result queue protected by the loader mutex.
		 * @return True when one result has the same world/body/component/revision/coordinate identity.
		 */
		bool requestHasCompletedResult(
			const ChunkGenerationRequest& request,
			const DynamicArray<GeneratedChunkData>& completed)
		{
			for(const GeneratedChunkData& result : completed)
			{
				if(result.generation_id == request.generation_id
					&& result.component_slot_id == request.component_slot_id
					&& result.component_generation_id == request.component_generation_id
					&& result.terrain_revision == request.terrain_revision
					&& result.world_body_id == request.world_body_id
					&& result.grid_coordinate == request.grid_coordinate) return true;
			}
			return false;
		}
	}

	/**
	 * @brief Stops and joins the worker, then allows its request/result arrays to be destroyed.
	 * The destructor delegates to stop(), making repeated shutdown safe.
	 */
	ChunkLoader::~ChunkLoader()
	{
		stop();
	}

	/**
	 * @brief Starts the single worker unless a worker thread is already joinable.
	 * The worker waits on the condition variable until requests arrive or stop is called.
	 */
	void ChunkLoader::start()
	{
		std::lock_guard<std::mutex> guard(mutex);
		if(worker.joinable()) return;
		stopping = false;
		worker = std::thread(&ChunkLoader::run, this);
	}

	/**
	 * @brief Requests worker shutdown, wakes both wait conditions, joins, and clears queued data.
	 * An in-progress chunk generation is not interrupted; run() discards its result
	 * after observing stopping. Calling stop() while not started is a no-op.
	 */
	void ChunkLoader::stop()
	{
		{
			std::lock_guard<std::mutex> guard(mutex);
			if(!worker.joinable()) return;
			stopping = true;
		}
		order_available.notify_all();
		result_space_available.notify_all();
		worker.join();
		std::lock_guard<std::mutex> guard(mutex);
		requests.clear();
		completed.clear();
		has_active_request = false;
		stopping = false;
	}

	/**
	 * @brief Replaces pending work with the latest request snapshot while deduplicating active/completed jobs.
	 * @param generation_id Current World lifetime token; a change discards old completed results.
	 * @param new_requests Requests desired by all planet components for this world state.
	 * Requests with stale tokens, the active request's key, or a result already in
	 * the completed queue are omitted. If the filtered queue is unchanged, no
	 * allocation or worker wake-up is needed.
	 */
	void ChunkLoader::updatePendingRequests(
		uint64 generation_id,
		const DynamicArray<ChunkGenerationRequest>& new_requests)
	{
		{
			std::lock_guard<std::mutex> guard(mutex);
			if(current_generation_id != generation_id)
			{
				current_generation_id = generation_id;
				completed.clear();
				result_space_available.notify_all();
			}

			/**
			 * @brief Avoids rebuilding the pending array when the filtered request snapshot is unchanged.
			 * This keeps repeated per-frame updates from causing unnecessary allocations and scans.
			 */
			size_t queued_index = 0;
			bool same_queue = true;
			for(const ChunkGenerationRequest& request : new_requests)
			{
				if(request.generation_id != current_generation_id
					|| (has_active_request && sameRequestKey(active_request, request))
					|| requestHasCompletedResult(request, completed)) continue;
				if(queued_index >= requests.size() || !sameRequestKey(requests[queued_index], request))
				{
					same_queue = false;
					break;
				}
				queued_index++;
			}
			if(same_queue && queued_index == requests.size()) return;

			requests.clear();
			for(const ChunkGenerationRequest& request : new_requests)
			{
				if(request.generation_id != current_generation_id
					|| (has_active_request && sameRequestKey(active_request, request))
					|| requestHasCompletedResult(request, completed)) continue;
				requests.push(request);
			}
		}
		order_available.notify_one();
	}

	/**
	 * @brief Moves the oldest completed result for the current world generation to the caller.
	 * @param result Receives the moved CPU-side geometry and request identity.
	 * @return True when a current-generation result was transferred; stale results
	 *         are removed and skipped, and false means the queue is empty afterward.
	 */
	bool ChunkLoader::tryTakeCompleted(GeneratedChunkData& result)
	{
		std::lock_guard<std::mutex> guard(mutex);

		while(!completed.empty())
		{
			GeneratedChunkData candidate = static_cast<GeneratedChunkData&&>(completed[0]);
			for(size_t index = 1; index < completed.size(); index++)
			{
				completed[index - 1] = static_cast<GeneratedChunkData&&>(completed[index]);
			}
			completed.resize(completed.size() - 1);
			if(candidate.generation_id != current_generation_id) continue;
			result = static_cast<GeneratedChunkData&&>(candidate);
			result_space_available.notify_one();
			return true;
		}
		return false;
	}

	/**
	 * @brief Waits for jobs, generates CPU-only chunk data outside the mutex, and publishes valid results.
	 *
	 * The worker removes the oldest request and marks it active while locked,
	 * performs scalar sampling, mesh extraction, and save I/O without holding the
	 * queue mutex, then waits until the two-entry result queue has room. Results
	 * from an obsolete world token or during shutdown are discarded. OpenGL work
	 * and WorldData resource allocation happen later on the main thread.
	 */
	void ChunkLoader::run()
	{
		while(true)
		{
			ChunkGenerationRequest request;
			{
				std::unique_lock<std::mutex> lock(mutex);
				order_available.wait(lock, [this] { return stopping || !requests.empty(); });
				if(stopping) return;
				request = requests[0];
				for(size_t index = 1; index < requests.size(); index++)
				{
					requests[index - 1] = static_cast<ChunkGenerationRequest&&>(requests[index]);
				}
				requests.resize(requests.size() - 1);
				active_request = request;
				has_active_request = true;
			}

			GeneratedChunkData result = generatePlanetChunk(request);
			{
				std::unique_lock<std::mutex> lock(mutex);
				result_space_available.wait(lock, [this]
				{
					return stopping || completed.size() < max_completed_chunks;
				});
				has_active_request = false;
				if(!stopping && request.generation_id == current_generation_id)
				{
					completed.pushMove(result);
				}
			}
		}
	}
}
