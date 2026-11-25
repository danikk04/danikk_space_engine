#include <danikk_space_engine/systems/build_object/system.h>

#include <danikk_framework/log.h>
#include <danikk_engine/danikk_engine.h>
#include <danikk_space_engine/assets/collection.h>
#include <danikk_space_engine/world.h>
#include <danikk_space_engine/systems/camera/camera.h>
#include <danikk_space_engine/systems/world_body/system.h>
#include <danikk_space_engine/systems/build_part/system.h>
#include <danikk_space_engine/systems/telemetry/telemetry.h>
#include <danikk_space_engine/systems/world_body/world_body.h>

namespace danikk_space_engine
{
	void BuildObjectSystem::init()
	{
		danikk_framework::formatLogInfo("Init BuildObjectSystem");
	}

	void BuildObjectSystem::shutdown()
	{
		danikk_framework::formatLogInfo("Shutdown BuildObjectSystem");
	}

	BuildObjectId BuildObjectSystem::createBuildObject(WorldBodyId world_body_id)
	{
		if(!world.getWorldBodySystem().isWorldBodyActive(world_body_id))
			danikk_engine::formatFatalError("Cannot attach a BuildObject to inactive WorldBody %", world_body_id);
		while(world_body_to_build_object.size() <= world_body_id)
			world_body_to_build_object.push(static_cast<BuildObjectId>(-1));
		if(world_body_to_build_object[world_body_id] != static_cast<BuildObjectId>(-1))
			danikk_engine::formatFatalError("WorldBody % already has a BuildObject", world_body_id);
		const BuildObjectId id = build_objects.create();
		BuildObject& object = build_objects[id];
		object.world_body_id = world_body_id;
		object.generation_id = next_generation_id++;
		if(next_generation_id == 0) next_generation_id++;
		world_body_to_build_object[world_body_id] = id;
		return id;
	}

	void BuildObjectSystem::destroyBuildObject(BuildObjectId id)
	{
		if(!build_objects.isActive(id)) return;
		BuildObject& object = build_objects[id];
		world.getBuildPartSystem().destroyBuildPartsForObject(id);
		if(object.detail_mesh.mesh != nullptr)
		{
			world.getTelemetrySystem().mesh_generation_telemetry.recordMeshDeletion(
				MeshDeletionReason::world_body_destroyed);
			destroyMeshRenderData(object.detail_mesh);
		}
		object.detail_mesh_bounds_valid = false;
		const WorldBodyId body_id = object.world_body_id;
		if(body_id < world_body_to_build_object.size()
			&& world_body_to_build_object[body_id] == id)
			world_body_to_build_object[body_id] = static_cast<BuildObjectId>(-1);
		build_objects.free(id);
	}

	void BuildObjectSystem::destroyBuildObjectForWorldBody(WorldBodyId world_body_id)
	{
		const BuildObjectId id = findBuildObjectId(world_body_id);
		if(id != static_cast<BuildObjectId>(-1)) destroyBuildObject(id);
	}

	bool BuildObjectSystem::hasBuildObject(WorldBodyId world_body_id) const
	{
		return findBuildObjectId(world_body_id) != static_cast<BuildObjectId>(-1);
	}

	BuildObjectId BuildObjectSystem::findBuildObjectId(WorldBodyId world_body_id) const
	{
		if(world_body_id >= world_body_to_build_object.size()) return static_cast<BuildObjectId>(-1);
		const BuildObjectId id = world_body_to_build_object[world_body_id];
		return build_objects.isActive(id) ? id : static_cast<BuildObjectId>(-1);
	}

	bool BuildObjectSystem::isBuildObjectActive(BuildObjectId id) const { return build_objects.isActive(id); }

	BuildObject& BuildObjectSystem::getBuildObject(BuildObjectId id)
	{
		if(!build_objects.isActive(id)) danikk_engine::formatFatalError("Invalid BuildObject ID %", id);
		return build_objects[id];
	}

	const BuildObject& BuildObjectSystem::getBuildObject(BuildObjectId id) const
	{
		if(!build_objects.isActive(id)) danikk_engine::formatFatalError("Invalid BuildObject ID %", id);
		return build_objects[id];
	}

	const DynamicArray<BuildObjectId>& BuildObjectSystem::getUsedBuildObjectIds() const
	{
		return build_objects.usedIds();
	}

	void BuildObjectSystem::renderMeshes() const
	{
		if(!world.getCameraSystem().isActive()) return;
		const CameraSystem& camera = world.getCameraSystem();
		const glm::dvec3 camera_position = camera.getWorldPosition();
		static_asset_collection.base3d_shader.use();
		camera.applyProjection();
		setViewMatrix(glm::lookAt(vec3(0.0f), camera.getFront(), camera.getUp()));
		for(BuildObjectId object_id : getUsedBuildObjectIds())
		{
			const BuildObject& object = getBuildObject(object_id);
			const WorldBodyId body_id = object.world_body_id;
			if(!world.getWorldBodySystem().isWorldBodyActive(body_id)) continue;
			const WorldBody& body = world.getWorldBodySystem().getWorldBody(body_id);
			if(glm::length(body.position - camera_position) > 10000.0) continue;
			if(object.detail_mesh.mesh == nullptr) continue;

			const glm::dvec3 relative_position = body.position
				+ body.rotation * object.detail_mesh_position - camera_position;
			mat4 rotation(1.0f);
			rotation[0] = vec4(vec3(body.rotation * glm::dvec3(1.0, 0.0, 0.0)), 0.0f);
			rotation[1] = vec4(vec3(body.rotation * glm::dvec3(0.0, 1.0, 0.0)), 0.0f);
			rotation[2] = vec4(vec3(body.rotation * glm::dvec3(0.0, 0.0, 1.0)), 0.0f);
			drawMeshRenderData(object.detail_mesh,
				glm::translate(mat4(1.0f), vec3(relative_position)) * rotation);
		}
	}

	void BuildObjectSystem::release()
	{
		while(build_objects.usedIds().size() > 0)
			destroyBuildObject(build_objects.usedIds()[build_objects.usedIds().size() - 1]);
		build_objects.releaseMemory();
		world_body_to_build_object = DynamicArray<BuildObjectId>();
	}
}
