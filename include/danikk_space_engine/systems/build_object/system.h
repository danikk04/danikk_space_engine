#pragma once

#include <danikk_space_engine/fwd.h>
#include <danikk_space_engine/resource_pool.h>
#include <danikk_space_engine/systems/build_object/data.h>

namespace danikk_space_engine
{
	/** @brief Owns construction metadata that extends selected WorldBodies. */
	class BuildObjectSystem
	{
		World& world;
		WorldResourcePool<BuildObject, BuildObjectId> build_objects{"BuildObject"};
		/** @brief Direct WorldBodyId-to-BuildObjectId lookup; absent entries use the invalid ID. */
		DynamicArray<BuildObjectId> world_body_to_build_object;
		uint64 next_generation_id = 1;

	public:
		explicit BuildObjectSystem(World& world) : world(world) {}
		void init();
		void shutdown();
		BuildObjectId createBuildObject(WorldBodyId world_body_id);
		void destroyBuildObject(BuildObjectId id);
		void destroyBuildObjectForWorldBody(WorldBodyId world_body_id);
		bool hasBuildObject(WorldBodyId world_body_id) const;
		BuildObjectId findBuildObjectId(WorldBodyId world_body_id) const;
		bool isBuildObjectActive(BuildObjectId id) const;
		BuildObject& getBuildObject(BuildObjectId id);
		const BuildObject& getBuildObject(BuildObjectId id) const;
		const DynamicArray<BuildObjectId>& getUsedBuildObjectIds() const;
		/** @brief Draws each resident combined mesh when its owning WorldBody is within 10 km. */
		void renderMeshes() const;
		void release();
	};
}
