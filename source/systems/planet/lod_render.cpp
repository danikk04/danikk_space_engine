#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <memory>
#include <new>

#include <danikk_space_engine/profiler.h>
#include <danikk_space_engine/systems/planet/system.h>
#include <danikk_space_engine/world.h>
#include <danikk_space_engine/material.h>
#include <danikk_space_engine/assets/collection.h>

#include <danikk_engine/texture_methods.h>
#include <danikk_framework/log.h>

namespace danikk_space_engine
{
	namespace
	{
		constexpr uint8 first_global_level = 4;

mat4 createPlanetTransform(const World& world, const WorldBody& body,
			const glm::dvec3& local_origin = glm::dvec3(0.0))
		{
			const glm::dvec3 relative_position = body.position + body.rotation * local_origin
				- world.camera.getWorldPosition();
			mat4 rotation(1.0f);
			rotation[0] = vec4(vec3(body.rotation * glm::dvec3(1.0, 0.0, 0.0)), 0.0f);
			rotation[1] = vec4(vec3(body.rotation * glm::dvec3(0.0, 1.0, 0.0)), 0.0f);
			rotation[2] = vec4(vec3(body.rotation * glm::dvec3(0.0, 0.0, 1.0)), 0.0f);
			return glm::translate(mat4(1.0f), vec3(relative_position)) * rotation;
		}
	}

	/** @copydoc PlanetSystem::frame */
	void PlanetSystem::frame(PlanetRenderPass pass) const
	{
		DSE_WATCH();
		if(!world.camera.isActive()) return;
		for(const PlanetData& planet : planets)
		{
			if(planet.lod_level == 0
				|| (pass == PlanetRenderPass::near_depth && planet.lod_level >= first_global_level)) continue;
			if(!world.world_body_data.isWorldBodyActive(planet.world_body_id)) continue;
			const WorldBody& body = world.world_body_data.getWorldBody(planet.world_body_id);
			const uint8 level_index = planet.lod_level;
			if(level_index >= planet.levels.size()) continue;
			const PlanetChunkLevel& level = planet.levels[level_index];
			if(level.mesh == nullptr) continue;
			setWorldMatrix(createPlanetTransform(world, body,
				level.camera_local ? level.render_mesh_origin : glm::dvec3(0.0)));
			for(const PlanetLodMaterialRange& range : level.material_ranges)
			{
				if(range.index_count == 0) continue;
				MaterialDefinition& material = getMaterial(range.material_id);
				if(material.texture.isNull()) white_texture.bind();
				else material.texture.bind();
				setDrawColor(vec4(material.color, 1.0f));
				level.mesh->drawRange(range.first_index, range.index_count);
			}
		}
	}

	/** @copydoc PlanetSystem::shouldDrawPlanetChunks */
	bool PlanetSystem::shouldDrawPlanetChunks(WorldBodyId world_body_id) const
	{
		DSE_WATCH();
		for(const PlanetData& planet : planets)
		{
			if(planet.world_body_id != world_body_id) continue;
			return planet.lod_level == 0;
		}
		return true;
	}

	/** @copydoc PlanetSystem::getCurrentLodLevel */
	bool PlanetSystem::getCurrentLodLevel(WorldBodyId world_body_id, uint8& lod_level) const
	{
		DSE_WATCH();
		for(const PlanetData& planet : planets)
		{
			if(planet.world_body_id != world_body_id) continue;
			lod_level = planet.lod_level;
			return true;
		}
		return false;
	}

	/** @copydoc PlanetSystem::getRegularChunkIds */
	const DynamicArray<ChunkId>* PlanetSystem::getRegularChunkIds(WorldBodyId world_body_id) const
	{
		DSE_WATCH();
		for(const PlanetData& planet : planets)
			if(planet.world_body_id == world_body_id && planet.levels.size() > 0)
				return &planet.levels[0].chunk_ids;
		return nullptr;
	}


}
