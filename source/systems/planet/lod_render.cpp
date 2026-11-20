#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <memory>
#include <new>

#include <danikk_space_engine/profiler.h>
#include <danikk_space_engine/systems/planet/planet.h>
#include <danikk_space_engine/world.h>
#include <danikk_space_engine/material.h>
#include <danikk_space_engine/assets/collection.h>

#include <danikk_engine/texture_methods.h>
#include <danikk_framework/log.h>

namespace danikk_space_engine
{
	namespace
	{
		constexpr uint8 local_patch_level = 1;

bool isLevelReady(const PlanetChunkLevel& level)
		{
			if(!level.initialized || level.mesh == nullptr
				|| level.patches.size() != (level.camera_local ? 1 : planet_lod_face_count)) return false;
			for(const PlanetLodPatch& patch : level.patches)
				if(!patch.ready) return false;
			return true;
		}

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

	/** @copydoc PlanetSystem::drawLodFar */
	void PlanetSystem::drawLodFar() const
	{
		DSE_WATCH();
		if(!world.camera.isActive()) return;
		for(const PlanetChunkRenderData& planet : chunk_render_data)
		{
			if(planet.render_level <= local_patch_level) continue;
			if(!world.world_body_data.isWorldBodyActive(planet.world_body_id)) continue;
			const WorldBody& body = world.world_body_data.getWorldBody(planet.world_body_id);
			setWorldMatrix(createPlanetTransform(world, body));
			const uint8 global_level_index = planet.render_level;
			if(global_level_index >= planet_chunk_level_count
				|| !isLevelReady(planet.levels[global_level_index])) continue;
			const PlanetChunkLevel& level = planet.levels[global_level_index];
			if(level.mesh == nullptr) continue;
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

	/** @copydoc PlanetSystem::drawLodNear */
	void PlanetSystem::drawLodNear() const
	{
		DSE_WATCH();
		if(!world.camera.isActive()) return;
		for(const PlanetChunkRenderData& planet : chunk_render_data)
		{
			if(planet.render_level != local_patch_level
				|| !world.world_body_data.isWorldBodyActive(planet.world_body_id)) continue;
			const PlanetChunkLevel& level = planet.levels[local_patch_level];
			if(!level.initialized || level.mesh == nullptr) continue;
			const WorldBody& body = world.world_body_data.getWorldBody(planet.world_body_id);
			setWorldMatrix(createPlanetTransform(world, body, level.mesh_origin));
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
		for(const PlanetChunkRenderData& planet : chunk_render_data)
		{
			if(planet.world_body_id != world_body_id) continue;
			return planet.render_level == 0;
		}
		return true;
	}

	/** @copydoc PlanetSystem::getRegularChunkIds */
	const DynamicArray<ChunkId>* PlanetSystem::getRegularChunkIds(WorldBodyId world_body_id) const
	{
		DSE_WATCH();
		for(const PlanetChunkRenderData& planet : chunk_render_data)
			if(planet.world_body_id == world_body_id && planet.levels.size() > 0)
				return &planet.levels[0].chunk_ids;
		return nullptr;
	}


}
