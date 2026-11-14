#include <cmath>
#include <filesystem>
#include <fstream>
#include <string>

#include <danikk_space_engine/world/chunk_loader.h>
#include <danikk_space_engine/world/planet.h>

namespace danikk_space_engine
{
	namespace
	{
		constexpr uint32 chunk_cache_magic = 0x4443484Bu;
		constexpr uint32 chunk_cache_version = 1;

		struct ChunkCacheHeader
		{
			uint32 magic;
			uint32 version;
			uint32 face;
			int32 x;
			int32 y;
			uint32 point_count;
			uint32 index_count;
			double planet_radius;
		};

		bool sameRequestKey(const ChunkGenerationRequest& left, const ChunkGenerationRequest& right)
		{
			return left.generation_id == right.generation_id
				&& left.grid_coordinate == right.grid_coordinate;
		}

		std::filesystem::path cacheFilePath(const ChunkGenerationRequest& request)
		{
			return std::filesystem::current_path() / "cache" / "planet_surface"
				/ ("v1_" + std::to_string(request.grid_coordinate.z)
					+ "_" + std::to_string(request.grid_coordinate.x)
					+ "_" + std::to_string(request.grid_coordinate.y) + ".bin");
		}

		bool loadCachedChunk(const ChunkGenerationRequest& request, GeneratedChunkData& result)
		{
			std::ifstream input(cacheFilePath(request), std::ios::binary);
			if(!input) return false;
			ChunkCacheHeader header{};
			input.read(reinterpret_cast<char*>(&header), sizeof(header));
			if(!input || header.magic != chunk_cache_magic || header.version != chunk_cache_version
				|| header.face != static_cast<uint32>(request.grid_coordinate.z)
				|| header.x != request.grid_coordinate.x || header.y != request.grid_coordinate.y
				|| header.planet_radius != default_planet_radius
				|| header.point_count > 10000 || header.index_count > 1000000
				|| header.index_count % 3 != 0)
			{
				return false;
			}

			std::vector<vec3> point_positions(header.point_count);
			for(vec3& position : point_positions)
			{
				input.read(reinterpret_cast<char*>(&position.x), sizeof(float));
				input.read(reinterpret_cast<char*>(&position.y), sizeof(float));
				input.read(reinterpret_cast<char*>(&position.z), sizeof(float));
			}
			std::vector<uint32> triangle_point_indices(header.index_count);
			input.read(reinterpret_cast<char*>(triangle_point_indices.data()),
				static_cast<std::streamsize>(header.index_count * sizeof(uint32)));
			if(!input) return false;

			for(uint32 point_index : triangle_point_indices)
			{
				if(point_index >= point_positions.size()) return false;
			}
			result.point_positions = static_cast<std::vector<vec3>&&>(point_positions);
			result.triangle_point_indices = static_cast<std::vector<uint32>&&>(triangle_point_indices);
			return true;
		}

		void saveCachedChunk(const ChunkGenerationRequest& request, const GeneratedChunkData& result)
		{
			std::error_code error;
			const std::filesystem::path file_path = cacheFilePath(request);
			std::filesystem::create_directories(file_path.parent_path(), error);
			if(error) return;
			std::ofstream output(file_path, std::ios::binary | std::ios::trunc);
			if(!output) return;
			const ChunkCacheHeader header{
				chunk_cache_magic,
				chunk_cache_version,
				static_cast<uint32>(request.grid_coordinate.z),
				request.grid_coordinate.x,
				request.grid_coordinate.y,
				static_cast<uint32>(result.point_positions.size()),
				static_cast<uint32>(result.triangle_point_indices.size()),
				default_planet_radius};
			output.write(reinterpret_cast<const char*>(&header), sizeof(header));
			for(const vec3& position : result.point_positions)
			{
				output.write(reinterpret_cast<const char*>(&position.x), sizeof(float));
				output.write(reinterpret_cast<const char*>(&position.y), sizeof(float));
				output.write(reinterpret_cast<const char*>(&position.z), sizeof(float));
			}
			output.write(reinterpret_cast<const char*>(result.triangle_point_indices.data()),
				static_cast<std::streamsize>(result.triangle_point_indices.size() * sizeof(uint32)));
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
		const std::vector<ChunkGenerationRequest>& new_requests)
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
						&& result.grid_coordinate == request.grid_coordinate) already_queued = true;
				}
				for(const ChunkGenerationRequest& queued : requests)
				{
					if(sameRequestKey(queued, request)) already_queued = true;
				}
				if(!already_queued && request.generation_id == current_generation_id) requests.push_back(request);
			}
		}
		order_available.notify_one();
	}

	bool ChunkLoader::tryTakeCompleted(GeneratedChunkData& result)
	{
		std::lock_guard<std::mutex> guard(mutex);
		while(!completed.empty())
		{
			GeneratedChunkData candidate = static_cast<GeneratedChunkData&&>(completed.front());
			completed.erase(completed.begin());
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
				request = requests.front();
				requests.erase(requests.begin());
				active_request = request;
				has_active_request = true;
			}

			GeneratedChunkData result = generate(request);
			{
				std::lock_guard<std::mutex> guard(mutex);
				has_active_request = false;
				if(!stopping && request.generation_id == current_generation_id)
				{
					completed.push_back(static_cast<GeneratedChunkData&&>(result));
				}
			}
		}
	}

	/*
	 * Builds a curved tangent-plane patch using only request data and local buffers. The worker
	 * never reads or writes WorldData and never touches OpenGL. It samples the planet scalar field,
	 * stores point positions relative to the chunk origin, and returns triangle corner indices for
	 * the main thread to publish into the world's ID-based arrays.
	 */
	GeneratedChunkData ChunkLoader::generate(const ChunkGenerationRequest& request)
	{
		GeneratedChunkData result;
		result.generation_id = request.generation_id;
		result.world_body_id = request.world_body_id;
		result.grid_coordinate = request.grid_coordinate;
		result.chunk_position = request.chunk_position;
		if(loadCachedChunk(request, result)) return result;

		constexpr double cell_size = 4.0;
		const size_t cells_x = static_cast<size_t>(std::ceil(request.width / cell_size));
		const size_t cells_y = cells_x;
		const double step_x = request.width / static_cast<double>(cells_x);
		const double step_y = request.width / static_cast<double>(cells_y);
		const size_t points_per_row = cells_x + 1;
		result.point_positions.reserve(points_per_row * (cells_y + 1));
		result.triangle_point_indices.reserve(cells_x * cells_y * 6);

		for(size_t y_index = 0; y_index <= cells_y; y_index++)
		{
			for(size_t x_index = 0; x_index <= cells_x; x_index++)
			{
				const double x = request.x0 + static_cast<double>(x_index) * step_x;
				const double y = request.y0 + static_cast<double>(y_index) * step_y;
				const glm::dvec3 direction = glm::normalize(
					request.up * default_planet_radius
						+ request.east * x
						+ request.north * y);
				const glm::dvec3 sample_position = direction * default_planet_radius;
				const double terrain_height = samplePlanetScalarField(sample_position);
				const glm::dvec3 surface_position = direction * (default_planet_radius + terrain_height);
				result.point_positions.push_back(vec3(surface_position - request.chunk_position));
			}
		}

		for(size_t y_index = 0; y_index < cells_y; y_index++)
		{
			for(size_t x_index = 0; x_index < cells_x; x_index++)
			{
				const uint32 p00 = static_cast<uint32>(y_index * points_per_row + x_index);
				const uint32 p10 = p00 + 1;
				const uint32 p01 = p00 + static_cast<uint32>(points_per_row);
				const uint32 p11 = p01 + 1;
				result.triangle_point_indices.insert(result.triangle_point_indices.end(),
					{p00, p10, p11, p00, p11, p01});
			}
		}
		saveCachedChunk(request, result);
		return result;
	}
}
