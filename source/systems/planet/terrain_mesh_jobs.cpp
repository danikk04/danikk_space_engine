#include <limits>

#include <danikk_space_engine/systems/telemetry/telemetry.h>
#include <danikk_space_engine/systems/planet/system.h>
#include <danikk_space_engine/world.h>

#include <danikk_space_engine/world_body.h>
#include <danikk_space_engine/systems/mesh_generation/mesh_generation.h>
#include <danikk_space_engine/systems/camera/camera.h>

namespace danikk_space_engine
{
	const DynamicArray<MeshGenerationJob>& PlanetSystem::buildMeshGenerationJobBuffer(
		const DynamicArray<MeshGenerationJob>& candidates, size_t capacity)
	{
		DSE_WATCH();
		mesh_generation_job_candidate_buffer.clear();
		mesh_generation_job_candidate_distances.clear();
		if(capacity == 0 || candidates.empty()) return mesh_generation_job_candidate_buffer;
		mesh_generation_job_candidate_buffer.reserve(capacity);
		mesh_generation_job_candidate_distances.reserve(capacity);

		const bool has_camera = world.getCameraSystem().isActive();
		const glm::dvec3 camera_position = has_camera
			? world.getCameraSystem().getWorldPosition() : glm::dvec3(0.0);
		for(const MeshGenerationJob& candidate : candidates)
		{
			glm::dvec3 candidate_position(0.0);
			bool valid = true;
			if(candidate.source == MeshGenerationSource::planet_terrain_node)
			{
				const PlanetData* planet = findPlanet(candidate.planet_id);
				if(planet == nullptr || planet->generation_id != candidate.planet_generation_id)
					valid = false;
				else
				{
					const glm::dvec3 local_center = candidate.node_minimum
						+ glm::dvec3(candidate.node_width * 0.5);
					candidate_position = planet->position + planet->rotation * local_center;
				}
			}
			else if(candidate.source == MeshGenerationSource::build_parts)
			{
				if(!world.getWorldBodyData().isWorldBodyActive(candidate.world_body_id)) valid = false;
				else candidate_position = world.getWorldBodyData().getWorldBody(candidate.world_body_id).position;
			}
			if(!valid) continue;

			double distance_squared = std::numeric_limits<double>::infinity();
			if(has_camera)
			{
				const glm::dvec3 offset = candidate_position - camera_position;
				double distance_to_candidate = glm::length(offset);
				if(candidate.source == MeshGenerationSource::planet_terrain_node)
					distance_to_candidate = glm::max(0.0, distance_to_candidate
						- candidate.node_width * 0.8660254037844386);
				distance_squared = distance_to_candidate * distance_to_candidate;
			}
			size_t insert_index = 0;
			while(insert_index < mesh_generation_job_candidate_buffer.size()
				&& mesh_generation_job_candidate_distances[insert_index] <= distance_squared)
				insert_index++;
			if(insert_index >= capacity) continue;

			if(mesh_generation_job_candidate_buffer.size() < capacity)
			{
				mesh_generation_job_candidate_buffer.push(candidate);
				mesh_generation_job_candidate_distances.push(distance_squared);
			}
			for(size_t index = mesh_generation_job_candidate_buffer.size() - 1;
				index > insert_index; --index)
			{
				mesh_generation_job_candidate_buffer[index] =
					static_cast<MeshGenerationJob&&>(mesh_generation_job_candidate_buffer[index - 1]);
				mesh_generation_job_candidate_distances[index] = mesh_generation_job_candidate_distances[index - 1];
			}
			mesh_generation_job_candidate_buffer[insert_index] = candidate;
			mesh_generation_job_candidate_distances[insert_index] = distance_squared;
		}
		return mesh_generation_job_candidate_buffer;
	}

	/** @brief Adds this planet's pending node jobs to the frame candidate list. */
	void PlanetSystem::appendPlanetJobCandidates(PlanetData& planet)
	{
		DSE_WATCH();
		for(MeshGenerationJob& candidate : planet.pending_job_candidates)
		{
			candidate.planet_id = planet.id;
			candidate.planet_generation_id = planet.generation_id;
		}
		world.getMeshGenerationSystem().appendPlanetJobCandidates(planet.pending_job_candidates);
	}
}
