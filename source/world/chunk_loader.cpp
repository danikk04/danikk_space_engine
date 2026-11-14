#include <danikk_space_engine/world/chunk_loader.h>

namespace danikk_space_engine
{
	namespace
	{
		bool sameRequestKey(const ChunkGenerationRequest& left, const ChunkGenerationRequest& right)
		{
			return left.generation_id == right.generation_id
				&& left.component_slot_id == right.component_slot_id
				&& left.component_generation_id == right.component_generation_id
				&& left.world_body_id == right.world_body_id
				&& left.grid_coordinate == right.grid_coordinate;
		}
	}

	ChunkLoader::~ChunkLoader()
	{
		stop();
	}

	void ChunkLoader::start()
	{
		std::lock_guard<std::mutex> guard(mutex);
		if(worker.joinable()) return;
		stopping = false;
		worker = std::thread(&ChunkLoader::run, this);
	}

	void ChunkLoader::stop()
	{
		{
			std::lock_guard<std::mutex> guard(mutex);
			if(!worker.joinable()) return;
			stopping = true;
		}
		order_available.notify_all();
		worker.join();
		std::lock_guard<std::mutex> guard(mutex);
		requests.clear();
		completed.clear();
		has_active_request = false;
		stopping = false;
	}

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
			}
			requests.clear();
			for(const ChunkGenerationRequest& request : new_requests)
			{
				bool already_queued = has_active_request && sameRequestKey(active_request, request);
				for(const GeneratedChunkData& result : completed)
				{
					if(result.generation_id == request.generation_id
						&& result.component_slot_id == request.component_slot_id
						&& result.component_generation_id == request.component_generation_id
						&& result.grid_coordinate == request.grid_coordinate) already_queued = true;
				}
				for(const ChunkGenerationRequest& queued : requests)
				{
					if(sameRequestKey(queued, request)) already_queued = true;
				}
				if(!already_queued && request.generation_id == current_generation_id) requests.push(request);
			}
		}
		order_available.notify_one();
	}

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
			return true;
		}
		return false;
	}

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
				for(size_t index = 1; index < requests.size(); index++) requests[index - 1] = requests[index];
				requests.resize(requests.size() - 1);
				active_request = request;
				has_active_request = true;
			}

			GeneratedChunkData result = generatePlanetChunk(request);
			{
				std::lock_guard<std::mutex> guard(mutex);
				has_active_request = false;
				if(!stopping && request.generation_id == current_generation_id)
				{
					completed.pushMove(result);
				}
			}
		}
	}
}
