#include <limits>

#include <danikk_space_engine/systems/telemetry/telemetry.h>
#include <danikk_space_engine/systems/planet/system.h>
#include <danikk_space_engine/world.h>

namespace danikk_space_engine
{
	const DynamicArray<MeshGenerationJob>& PlanetSystem::buildMeshGenerationRequestBuffer(
		const DynamicArray<MeshGenerationJob>& candidates, size_t capacity)
	{
		DSE_WATCH();
		mesh_generation_request_buffer.clear();
		mesh_generation_request_distances.clear();
		if(capacity == 0 || candidates.empty()) return mesh_generation_request_buffer;
		mesh_generation_request_buffer.reserve(capacity);
		mesh_generation_request_distances.reserve(capacity);

		const bool has_camera = world.camera.isActive();
		const glm::dvec3 camera_position = has_camera
			? world.camera.getWorldPosition() : glm::dvec3(0.0);
		for(const MeshGenerationJob& request : candidates)
		{
			glm::dvec3 request_position(0.0);
			bool valid = true;
			if(request.source == MeshGenerationSource::planet_terrain_node)
			{
				const PlanetData* planet = findPlanet(request.planet_id);
				if(planet == nullptr || planet->generation_id != request.planet_generation_id)
					valid = false;
				else
				{
					const glm::dvec3 local_center = request.node_minimum
						+ glm::dvec3(request.node_width * 0.5);
					request_position = planet->position + planet->rotation * local_center;
				}
			}
			else if(request.source == MeshGenerationSource::build_parts)
			{
				if(!world.world_body_data.isWorldBodyActive(request.world_body_id)) valid = false;
				else request_position = world.world_body_data.getWorldBody(request.world_body_id).position;
			}
			if(!valid) continue;

			const double distance_squared = has_camera
				? glm::dot(request_position - camera_position, request_position - camera_position)
				: std::numeric_limits<double>::infinity();
			size_t insert_index = 0;
			while(insert_index < mesh_generation_request_buffer.size()
				&& mesh_generation_request_distances[insert_index] <= distance_squared)
				insert_index++;
			if(insert_index >= capacity) continue;

			if(mesh_generation_request_buffer.size() < capacity)
			{
				mesh_generation_request_buffer.push(request);
				mesh_generation_request_distances.push(distance_squared);
			}
			for(size_t index = mesh_generation_request_buffer.size() - 1;
				index > insert_index; --index)
			{
				mesh_generation_request_buffer[index] =
					static_cast<MeshGenerationJob&&>(mesh_generation_request_buffer[index - 1]);
				mesh_generation_request_distances[index] = mesh_generation_request_distances[index - 1];
			}
			mesh_generation_request_buffer[insert_index] = request;
			mesh_generation_request_distances[insert_index] = distance_squared;
		}
		return mesh_generation_request_buffer;
	}

	/** @brief Adds this planet's current adaptive-node requests to the frame candidate list. */
	void PlanetSystem::appendPlanetRequests(PlanetData& planet)
	{
		DSE_WATCH();
		for(MeshGenerationJob& request : planet.pending_requests)
		{
			request.planet_id = planet.id;
			request.planet_generation_id = planet.generation_id;
		}
		world.mesh_generation.appendPlanetRequests(planet.pending_requests);
	}
}
