#pragma once

#include <danikk_space_engine/fwd.h>
#include <danikk_space_engine/resource_pool.h>
#include <danikk_space_engine/systems/build_part/data.h>

namespace danikk_space_engine
{
	/** @brief Owns BuildPart instances and updates their BuildObject's combined mesh state. */
	class BuildPartSystem
	{
		World& world;
		WorldResourcePool<BuildPart, BuildPartId> build_parts{"BuildPart"};

	public:
		explicit BuildPartSystem(World& world) : world(world) {}
		void init();
		void shutdown();
		BuildPartId createBuildPart(BuildObjectId build_object_id, BuildPartShape shape,
			const glm::dvec3& body_local_position, const mat4& rotation, const vec3& size);
		void updateBuildPart(BuildPartId id, BuildPartShape shape,
			const glm::dvec3& body_local_position, const mat4& rotation, const vec3& size,
			float construction_progress);
		void destroyBuildPart(BuildPartId id);
		void destroyBuildPartsForObject(BuildObjectId build_object_id);
		bool isBuildPartActive(BuildPartId id) const;
		const BuildPart& getBuildPart(BuildPartId id) const;
		const DynamicArray<BuildPartId>& getUsedBuildPartIds() const;
		void release();
	};
}
