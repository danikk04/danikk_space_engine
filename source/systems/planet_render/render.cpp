#include <limits>

#include <danikk_space_engine/systems/telemetry/telemetry.h>
#include <danikk_space_engine/systems/planet_render/system.h>
#include <danikk_space_engine/assets/collection.h>
#include <danikk_space_engine/config.h>
#include <danikk_space_engine/systems/camera/camera.h>
#include <danikk_space_engine/systems/planet/system.h>
#include <danikk_space_engine/systems/world_body/system.h>
#include <danikk_space_engine/systems/world_body/world_body.h>
#include <danikk_space_engine/world.h>

namespace danikk_space_engine
{
	void PlanetRenderSystem::drawTerrainMeshes(const glm::dmat4& view,
		const glm::dmat4& projection, const glm::dvec3& camera_position)
	{
		static_asset_collection.planet_terrain_shader.use();
		setViewMatrix(mat4(view));
		setProjectionMatrix(mat4(projection));

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
		const CameraSystem& camera = world.getCameraSystem();
		static_asset_collection.base3d_shader.use();
		camera.frame();
		if(!camera.isActive())
		{
			setDepthRange(0.0f, 1.0f);
			return;
		}

		const glm::dvec3 camera_position = camera.getWorldPosition();
		const glm::dmat4 view = glm::lookAt(glm::dvec3(0.0),
			glm::dvec3(camera.getFront()), glm::dvec3(camera.getUp()));
		const glm::dmat4 standard_projection = glm::perspective(glm::radians(
			static_cast<double>(camera.vertical_fov_degrees)),
			static_cast<double>(screen_ratio_gz),
			static_cast<double>(camera.near_clip_distance),
			static_cast<double>(camera.far_clip_distance));

		double far_render_distance = camera.far_clip_distance;
		double closest_planet_surface_distance = std::numeric_limits<double>::max();
		for(const PlanetData& planet : world.getPlanetSystem().getPlanets())
		{
			const PlanetSettings& settings = world.getPlanetSystem().getPlanetSettings(planet.settings_id);
			const WorldBody& body = world.getWorldBodySystem().getWorldBody(planet.world_body_id);
			const glm::dvec3 local_camera = glm::conjugate(body.rotation)
				* (camera_position - body.position);
			const double surface_height = world.getPlanetSystem().getPlanetHeightAboveSurface(
				planet.id, local_camera);
			closest_planet_surface_distance = glm::min(
				closest_planet_surface_distance, std::abs(surface_height));
			far_render_distance = glm::max(far_render_distance,
				glm::length(body.position - camera_position) + settings.radius);
		}

		if(far_render_distance > camera.far_clip_distance * 1.05)
		{
			float near_clip = camera.far_clip_distance * 0.95f;
			if(closest_planet_surface_distance < std::numeric_limits<double>::max())
				near_clip = glm::min(near_clip, static_cast<float>(glm::max(
					static_cast<double>(camera.near_clip_distance), closest_planet_surface_distance * 0.5)));
			const float far_clip = static_cast<float>(glm::min(far_render_distance * 1.02,
				static_cast<double>(std::numeric_limits<float>::max() / 2.0f)));
			const glm::dmat4 far_projection = glm::perspective(glm::radians(
				static_cast<double>(camera.vertical_fov_degrees)),
				static_cast<double>(screen_ratio_gz), static_cast<double>(near_clip),
				static_cast<double>(far_clip));
			setDepthRange(0.5f, 1.0f);
			drawTerrainMeshes(view, far_projection, camera_position);
			setDepthRange(0.0f, 0.5f);
			drawTerrainMeshes(view, standard_projection, camera_position);
		}
		else
		{
			setDepthRange(0.0f, 1.0f);
			drawTerrainMeshes(view, standard_projection, camera_position);
		}

		static_asset_collection.base3d_shader.use();
		setViewMatrix(mat4(view));
		setProjectionMatrix(mat4(standard_projection));
	}
}
