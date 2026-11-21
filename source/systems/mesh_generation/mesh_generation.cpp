#include <danikk_space_engine/profiler.h>
#include <danikk_space_engine/systems/mesh_generation/mesh_generation.h>
#include <danikk_space_engine/world.h>
#include <danikk_space_engine/component/point/build_part.h>
#include <danikk_space_engine/component/world_body/world_body.h>
#include <danikk_framework/log.h>

#include "internal.h"

namespace danikk_space_engine
{
	MeshGenerationSystem::MeshGenerationSystem(World& world)
		: world(world), worker(std::make_unique<mesh_generation_internal::Worker>(world)) {}

	MeshGenerationSystem::~MeshGenerationSystem() { shutdown(); }

	bool MeshGenerationSystem::getAverageGenerationTimeMilliseconds(float& average_time) const
	{
		return worker != nullptr && worker->getAverageGenerationTimeMilliseconds(average_time);
	}

	/** @copydoc MeshGenerationSystem::init */
	void MeshGenerationSystem::init()
	{
		DSE_WATCH();
		if(initialized) return;
		danikk_framework::formatLogInfo("Init MeshGenerationSystem");
		worker->start();
		initialized = true;
		generation_id++;
		if(generation_id == 0) generation_id++;
		requests.clear();
		requests_dirty = false;
		worker->update(generation_id, requests);
	}

	/** @copydoc MeshGenerationSystem::shutdown */
	void MeshGenerationSystem::shutdown()
	{
		DSE_WATCH();
		if(!initialized) return;
		danikk_framework::formatLogInfo("Shutdown MeshGenerationSystem");
		worker->stop();
		initialized = false;
		generation_id++;
		requests.clear();
		requests_dirty = false;
	}

	/** @copydoc MeshGenerationSystem::replacePlanetRequests */
	void MeshGenerationSystem::replacePlanetRequests(WorldBodyId world_body_id,
		uint64 planet_generation_id, const DynamicArray<MeshGenerationRequest>& planet_requests)
	{
		DSE_WATCH();
		for(size_t index = 0; index < requests.size();)
		{
			const MeshGenerationRequest& queued = requests[index];
			if(queued.source != MeshGenerationSource::planet_terrain_node
				|| queued.world_body_id != world_body_id
				|| queued.planet_generation_id != planet_generation_id)
			{
				index++;
				continue;
			}
			for(size_t shift = index + 1; shift < requests.size(); shift++)
				requests[shift - 1] = static_cast<MeshGenerationRequest&&>(requests[shift]);
			requests.resize(requests.size() - 1);
		}
		for(const MeshGenerationRequest& request : planet_requests)
		{
			MeshGenerationRequest queued = request;
			queued.generation_id = generation_id;
			requests.pushMove(queued);
		}
		requests_dirty = true;
	}

	/** @copydoc MeshGenerationSystem::cancelPlanetRequests */
	void MeshGenerationSystem::cancelPlanetRequests(WorldBodyId world_body_id,
		uint64 planet_generation_id)
	{
		DSE_WATCH();
		worker->cancel(world_body_id, planet_generation_id);
		for(size_t index = 0; index < requests.size();)
		{
			const MeshGenerationRequest& request = requests[index];
			if(request.source != MeshGenerationSource::planet_terrain_node
				|| request.world_body_id != world_body_id
				|| request.planet_generation_id != planet_generation_id)
			{
				index++;
				continue;
			}
			for(size_t shift = index + 1; shift < requests.size(); shift++)
				requests[shift - 1] = static_cast<MeshGenerationRequest&&>(requests[shift]);
			requests.resize(requests.size() - 1);
			requests_dirty = true;
		}
	}

	void MeshGenerationSystem::invalidateBodyDetailMesh(WorldBodyId world_body_id)
	{
		DSE_WATCH();
		if(!world.world_body_data.isWorldBodyActive(world_body_id)) return;
		WorldBody& body = world.world_body_data.getWorldBody(world_body_id);
		body.detail_mesh_dirty = true;
		body.detail_mesh_queued = false;
		if(++body.detail_mesh_revision == 0) ++body.detail_mesh_revision;
	}

	/** @copydoc MeshGenerationSystem::queueDirtyBuildPartMeshes */
	void MeshGenerationSystem::queueDirtyBuildPartMeshes()
	{
		DSE_WATCH();
		for(size_t index = 0; index < requests.size();)
		{
			const MeshGenerationRequest& request = requests[index];
			if(request.source != MeshGenerationSource::build_parts
				|| (world.world_body_data.isWorldBodyActive(request.world_body_id)
					&& world.world_body_data.getWorldBody(request.world_body_id).detail_mesh_revision
						== request.source_revision))
			{
				index++;
				continue;
			}
			for(size_t shift = index + 1; shift < requests.size(); shift++)
				requests[shift - 1] = static_cast<MeshGenerationRequest&&>(requests[shift]);
			requests.resize(requests.size() - 1);
			requests_dirty = true;
		}

		for(WorldBodyId world_body_id : world.world_body_data.getUsedWorldBodyIds())
		{
			WorldBody& body = world.world_body_data.getWorldBody(world_body_id);
			if(!body.detail_mesh_dirty || body.detail_mesh_queued) continue;
			destroyMeshRenderData(body.detail_mesh);

			MeshGenerationRequest request;
			request.source = MeshGenerationSource::build_parts;
			request.generation_id = generation_id;
			request.source_revision = body.detail_mesh_revision;
			request.world_body_id = world_body_id;
			for(size_t index = 0; index < requests.size();)
			{
				const MeshGenerationRequest& queued = requests[index];
				if(queued.source != MeshGenerationSource::build_parts
					|| queued.world_body_id != world_body_id)
				{
					index++;
					continue;
				}
				for(size_t shift = index + 1; shift < requests.size(); shift++)
					requests[shift - 1] = static_cast<MeshGenerationRequest&&>(requests[shift]);
				requests.resize(requests.size() - 1);
			}
			requests.pushMove(request);
			body.detail_mesh_dirty = false;
			body.detail_mesh_queued = true;
			requests_dirty = true;
		}
	}

	/** @copydoc MeshGenerationSystem::removeBuildPartMeshRequest */
	void MeshGenerationSystem::removeBuildPartMeshRequest(WorldBodyId world_body_id)
	{
		DSE_WATCH();
		for(size_t index = 0; index < requests.size(); index++)
		{
			const MeshGenerationRequest& request = requests[index];
			if(request.source != MeshGenerationSource::build_parts || request.world_body_id != world_body_id) continue;
			for(size_t shift = index + 1; shift < requests.size(); shift++)
				requests[shift - 1] = static_cast<MeshGenerationRequest&&>(requests[shift]);
			requests.resize(requests.size() - 1);
			requests_dirty = true;
			return;
		}
	}

	/** @copydoc MeshGenerationSystem::frame */
	void MeshGenerationSystem::frame()
	{
		DSE_WATCH();
		queueDirtyBuildPartMeshes();
		if(requests_dirty)
		{
			worker->update(generation_id, requests);
			requests_dirty = false;
		}

		GeneratedMesh generated;
		if(!worker->tryTake(generated) || generated.generation_id != generation_id) return;
		if(generated.source == MeshGenerationSource::planet_terrain_node
			&& world.world_body_data.isWorldBodyActive(generated.world_body_id))
		{
			world.planets.acceptGeneratedSurfaceNode(generated);
		}
		else if(generated.source == MeshGenerationSource::build_parts)
		{
			if(!world.world_body_data.isWorldBodyActive(generated.world_body_id)) return;
			WorldBody& body = world.world_body_data.getWorldBody(generated.world_body_id);
			if(body.detail_mesh_revision != generated.source_revision) return;
			createMeshRenderData(body.detail_mesh,
				generated.point_positions.data(), generated.point_positions.size(),
				generated.triangle_point_indices.data(), generated.triangle_point_indices.size(),
				generated.triangle_material_ids.data(), generated.triangle_material_ids.size());
			body.detail_mesh_position = generated.mesh_position;
			body.detail_mesh_queued = false;
			body.detail_mesh_bounds_valid = false;
			if(generated.point_positions.size() > 0)
			{
				vec3 minimum = generated.point_positions[0];
				vec3 maximum = generated.point_positions[0];
				for(const vec3& position : generated.point_positions)
				{
					minimum = glm::min(minimum, position);
					maximum = glm::max(maximum, position);
				}
				body.detail_mesh_bounds_center = (minimum + maximum) * 0.5f;
				body.detail_mesh_bounds_radius = 0.0f;
				for(const vec3& position : generated.point_positions)
					body.detail_mesh_bounds_radius = glm::max(body.detail_mesh_bounds_radius,
						glm::length(position - body.detail_mesh_bounds_center));
				body.detail_mesh_bounds_valid = true;
			}
			removeBuildPartMeshRequest(generated.world_body_id);
		}
	}
}
