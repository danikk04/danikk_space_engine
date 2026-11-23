#include <danikk_space_engine/systems/build_part/system.h>

#include <danikk_framework/log.h>
#include <danikk_engine/danikk_engine.h>
#include <danikk_engine/texture_methods.h>
#include <danikk_space_engine/assets/collection.h>
#include <danikk_space_engine/world.h>
#include <danikk_space_engine/systems/build_object/system.h>
#include <danikk_space_engine/systems/build_object/data.h>
#include <danikk_space_engine/systems/camera/camera.h>
#include <danikk_space_engine/systems/mesh_generation/mesh_generation.h>
#include <danikk_space_engine/systems/telemetry/watcher.h>
#include <danikk_space_engine/systems/world_body/system.h>
#include <danikk_space_engine/systems/world_body/world_body.h>
#include <danikk_space_engine/geometry/build_part.h>

#include <cstdlib>
#include <new>

namespace danikk_space_engine
{
	namespace
	{
		Mesh* build_part_meshes[static_cast<size_t>(BuildPartShape::count)]{};

		Mesh& getBuildPartMesh(BuildPartShape shape)
		{
			const size_t index = static_cast<size_t>(shape);
			if(index >= static_cast<size_t>(BuildPartShape::count))
				danikk_engine::formatFatalError("Invalid BuildPart shape %", index);
			if(build_part_meshes[index] != nullptr) return *build_part_meshes[index];

			const BuildPartGeometry& geometry = getBuildPartGeometry(shape);
			if(geometry.positions.size() == 0 || geometry.triangles.size() == 0)
				danikk_engine::formatFatalError("BuildPart shape % has no geometry", index);

			DynamicArray<DefaultVertex> vertices;
			DynamicArray<gl_point_index_t> indices;
			vertices.reserve(geometry.triangles.size() * 3);
			indices.reserve(geometry.triangles.size() * 3);
			for(const CubeTriangle& triangle : geometry.triangles)
			{
				const vec3 a = geometry.positions[triangle.points[0]];
				const vec3 b = geometry.positions[triangle.points[1]];
				const vec3 c = geometry.positions[triangle.points[2]];
				const vec3 normal = glm::normalize(glm::cross(b - a, c - a));
				const vec3 face_positions[3] = {a, b, c};
				for(const vec3& position : face_positions)
				{
					indices.push(static_cast<gl_point_index_t>(vertices.size()));
					vertices.pushCtor(position, normal, vec2(position.x + 0.5f, position.y + 0.5f));
				}
			}

			void* memory = malloc(sizeof(Mesh));
			if(memory == nullptr) std::abort();
			Mesh* mesh = new (memory) Mesh();
			mesh->setData(reinterpret_cast<const float*>(vertices.data()),
				vertices.size() * sizeof(DefaultVertex) / sizeof(float), indices.data(), indices.size());
			DefaultVertex::setAttributes();
			build_part_meshes[index] = mesh;
			return *mesh;
		}
	}

	void BuildPartSystem::init()
	{
		danikk_framework::formatLogInfo("Init BuildPartSystem");
	}

	void BuildPartSystem::shutdown()
	{
		danikk_framework::formatLogInfo("Shutdown BuildPartSystem");
	}

	BuildPartId BuildPartSystem::createBuildPart(BuildObjectId build_object_id,
		BuildPartShape shape, const glm::dvec3& body_local_position,
		const mat4& rotation, const vec3& size)
	{
		if(!world.getBuildObjectSystem().isBuildObjectActive(build_object_id))
			danikk_engine::formatFatalError("Cannot attach a BuildPart to inactive BuildObject %", build_object_id);
		const BuildPartId id = build_parts.create();
		BuildPart& part = build_parts[id];
		part.build_object_id = build_object_id;
		part.shape = shape;
		part.body_local_position = body_local_position;
		part.rotation = rotation;
		part.size = size;
		BuildObject& object = world.getBuildObjectSystem().getBuildObject(build_object_id);
		object.build_part_ids.push(id);
		world.getMeshGenerationSystem().invalidateBuildObjectMesh(build_object_id);
		return id;
	}

	void BuildPartSystem::updateBuildPart(BuildPartId id, BuildPartShape shape,
		const glm::dvec3& body_local_position, const mat4& rotation, const vec3& size,
		float construction_progress)
	{
		if(!build_parts.isActive(id)) danikk_engine::formatFatalError("Invalid BuildPart ID %", id);
		BuildPart& part = build_parts[id];
		if(!world.getBuildObjectSystem().isBuildObjectActive(part.build_object_id))
			danikk_engine::formatFatalError("BuildPart % references an inactive BuildObject", id);
		part.shape = shape;
		part.body_local_position = body_local_position;
		part.rotation = rotation;
		part.size = size;
		part.construction_progress = construction_progress;
		const BuildObject& object = world.getBuildObjectSystem().getBuildObject(part.build_object_id);
		if(world.getWorldBodySystem().isWorldBodyActive(object.world_body_id))
		{
			WorldBody& body = world.getWorldBodySystem().getWorldBody(object.world_body_id);
			double shape_radius = 0.0;
			for(const vec3& vertex : getBuildPartGeometry(shape).positions)
				shape_radius = glm::max(shape_radius, static_cast<double>(glm::length(
					rotation * vec4(vertex * size, 0.0f))));
			body.collision_radius = glm::max(body.collision_radius,
				glm::length(body_local_position) + shape_radius);
		}
		world.getMeshGenerationSystem().invalidateBuildObjectMesh(part.build_object_id);
	}

	void BuildPartSystem::destroyBuildPart(BuildPartId id)
	{
		if(!build_parts.isActive(id)) return;
		const BuildObjectId object_id = build_parts[id].build_object_id;
		if(world.getBuildObjectSystem().isBuildObjectActive(object_id))
		{
			BuildObject& object = world.getBuildObjectSystem().getBuildObject(object_id);
			for(size_t index = 0; index < object.build_part_ids.size(); ++index)
			{
				if(object.build_part_ids[index] != id) continue;
				for(size_t move = index + 1; move < object.build_part_ids.size(); ++move)
					object.build_part_ids[move - 1] = object.build_part_ids[move];
				object.build_part_ids.resize(object.build_part_ids.size() - 1);
				world.getMeshGenerationSystem().invalidateBuildObjectMesh(object_id);
				break;
			}
		}
		build_parts.free(id);
	}

	void BuildPartSystem::destroyBuildPartsForObject(BuildObjectId build_object_id)
	{
		if(!world.getBuildObjectSystem().isBuildObjectActive(build_object_id)) return;
		BuildObject& object = world.getBuildObjectSystem().getBuildObject(build_object_id);
		while(object.build_part_ids.size() > 0)
			destroyBuildPart(object.build_part_ids[object.build_part_ids.size() - 1]);
	}

	void BuildPartSystem::renderFallback()
	{
		DSE_WATCH();
		for(BuildObjectId object_id : world.getBuildObjectSystem().getUsedBuildObjectIds())
		{
			const BuildObject& object = world.getBuildObjectSystem().getBuildObject(object_id);
			if(object.detail_mesh.mesh != nullptr
				|| !world.getWorldBodySystem().isWorldBodyActive(object.world_body_id)) continue;
			const WorldBody& body = world.getWorldBodySystem().getWorldBody(object.world_body_id);
			const glm::dquat body_rotation = body.rotation;
			for(BuildPartId part_id : object.build_part_ids)
			{
				if(!build_parts.isActive(part_id)) continue;
				const BuildPart& part = build_parts[part_id];
				const glm::dvec3 relative_position = body.position
					+ body_rotation * part.body_local_position - world.getCameraSystem().getWorldPosition();
				mat4 rotation(1.0f);
				rotation[0] = vec4(vec3(body_rotation * glm::dvec3(1.0, 0.0, 0.0)), 0.0f);
				rotation[1] = vec4(vec3(body_rotation * glm::dvec3(0.0, 1.0, 0.0)), 0.0f);
				rotation[2] = vec4(vec3(body_rotation * glm::dvec3(0.0, 0.0, 1.0)), 0.0f);
				setWorldMatrix(glm::translate(mat4(1.0f), vec3(relative_position))
					* rotation * part.rotation * glm::scale(mat4(1.0f), part.size));
				white_texture.bind();
				setDrawColor(vec4(0.65f, 0.68f, 0.72f, 1.0f));
				getBuildPartMesh(part.shape).draw();
			}
		}
	}

	bool BuildPartSystem::isBuildPartActive(BuildPartId id) const { return build_parts.isActive(id); }

	const BuildPart& BuildPartSystem::getBuildPart(BuildPartId id) const
	{
		if(!build_parts.isActive(id)) danikk_engine::formatFatalError("Invalid BuildPart ID %", id);
		return build_parts[id];
	}

	const DynamicArray<BuildPartId>& BuildPartSystem::getUsedBuildPartIds() const
	{
		return build_parts.usedIds();
	}

	void BuildPartSystem::release()
	{
		while(build_parts.usedIds().size() > 0)
			destroyBuildPart(build_parts.usedIds()[build_parts.usedIds().size() - 1]);
		build_parts.releaseMemory();
	}
}
