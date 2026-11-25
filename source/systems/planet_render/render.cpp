#include <limits>

#include <danikk_space_engine/systems/telemetry/telemetry.h>
#include <danikk_space_engine/systems/planet_render/system.h>
#include <danikk_space_engine/assets/collection.h>
#include <danikk_space_engine/systems/camera/camera.h>
#include <danikk_space_engine/systems/planet/system.h>
#include <danikk_space_engine/world.h>

namespace danikk_space_engine
{
	void PlanetRenderSystem::drawTerrainMeshes(const glm::dvec3& camera_position)
	{
		for(const PlanetRenderData& planet : planet_render_data)
		{
			for(const PlanetTerrainRenderItem& item : planet.terrain_render_items)
			{
				if(item.node_id >= planet.terrain_tree.nodes.size()) continue;
				const MeshRenderData& mesh = planet.terrain_tree.nodes[item.node_id].mesh;
				if(mesh.mesh == nullptr) continue;
				const glm::dvec3 relative_position = item.world_position - camera_position;
				mat4 body_rotation(1.0f);
				body_rotation[0] = vec4(vec3(item.world_rotation * glm::dvec3(1.0, 0.0, 0.0)), 0.0f);
				body_rotation[1] = vec4(vec3(item.world_rotation * glm::dvec3(0.0, 1.0, 0.0)), 0.0f);
				body_rotation[2] = vec4(vec3(item.world_rotation * glm::dvec3(0.0, 0.0, 1.0)), 0.0f);
				const mat4 world_matrix = glm::translate(mat4(1.0f), vec3(relative_position)) * body_rotation;
				drawMeshRenderData(mesh, world_matrix);
			}
		}
	}

	/** @brief Draws planet terrain in far and near depth ranges when the planet exceeds camera range. */
	void PlanetRenderSystem::renderPlanetTerrainMeshes()
	{
		DSE_WATCH();
		CameraSystem& camera = world.getCameraSystem();
		PlanetSystem& planet_system = world.getPlanetSystem();
		if(!camera.isActive())
		{
			static_asset_collection.base3d_shader.use();
			setDepthRange(0.0f, 1.0f);
			return;
		}

		camera.setProjectionMode(CameraProjectionMode::near_clip, camera.near_clip_distance, camera.far_clip_distance);

		const glm::dvec3 camera_position = camera.getWorldPosition();
		const glm::dmat4 view = glm::lookAt(glm::dvec3(0.0), glm::dvec3(camera.getFront()), glm::dvec3(camera.getUp()));

		double far_render_distance;
		double closest_planet_surface_distance;
		planet_system.updateRenderDistances(camera_position, camera.far_clip_distance, far_render_distance, closest_planet_surface_distance);
		static_asset_collection.planet_terrain_shader.use();
		setViewMatrix(mat4(view));

		if(far_render_distance > camera.far_clip_distance * 1.05)
		{
			float near_clip = camera.far_clip_distance * 0.95f;
			if(closest_planet_surface_distance < std::numeric_limits<double>::max())
			{
				near_clip = glm::min(near_clip, static_cast<float>(glm::max(static_cast<double>(camera.near_clip_distance), closest_planet_surface_distance * 0.5)));
			}
			const float far_clip = static_cast<float>(glm::min(far_render_distance * 1.02, static_cast<double>(std::numeric_limits<float>::max() / 2.0f)));
			camera.setProjectionMode(CameraProjectionMode::far_clip, near_clip, far_clip);
			camera.applyProjection();
			setDepthRange(0.5f, 1.0f);
			drawTerrainMeshes(camera_position);
			camera.setProjectionMode(CameraProjectionMode::near_clip, camera.near_clip_distance, camera.far_clip_distance);
			camera.applyProjection();
			setDepthRange(0.0f, 0.5f);
			drawTerrainMeshes(camera_position);
		}
		else
		{
			camera.applyProjection();
			setDepthRange(0.0f, 1.0f);
			drawTerrainMeshes(camera_position);
		}

		static_asset_collection.base3d_shader.use();
		camera.setProjectionMode(CameraProjectionMode::near_clip, camera.near_clip_distance, camera.far_clip_distance);
		camera.applyProjection();
		setViewMatrix(mat4(view));
	}
}
