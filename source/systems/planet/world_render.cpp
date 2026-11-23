#include <cmath>
#include <limits>

#include <glm/gtc/matrix_transform.hpp>

#include <danikk_engine/danikk_engine.h>
#include <danikk_space_engine/systems/telemetry/telemetry.h>
#include <danikk_space_engine/systems/planet/system.h>
#include <danikk_space_engine/systems/camera/camera.h>
#include <danikk_space_engine/systems/build_object/system.h>
#include <danikk_space_engine/world.h>

namespace danikk_space_engine
{
	void PlanetSystem::renderWorldMeshes()
	{
		DSE_WATCH();
		const CameraSystem& camera = world.getCameraSystem();
		const glm::dvec3 camera_position = camera.getWorldPosition();
		double far_render_distance = camera.far_clip_distance;
		double closest_planet_surface_distance = std::numeric_limits<double>::max();
		for(const PlanetData& planet : planets)
		{
			const PlanetSettings& settings = getPlanetSettings(planet.settings_id);
			const glm::dvec3 local_camera = glm::conjugate(planet.rotation)
				* (camera_position - planet.position);
			const double surface_height = getPlanetHeightAboveSurface(planet.id, local_camera);
			closest_planet_surface_distance = glm::min(
				closest_planet_surface_distance, std::abs(surface_height));
			far_render_distance = glm::max(far_render_distance,
				glm::length(planet.position - camera_position) + settings.radius);
		}
		far_render_distance = glm::max(far_render_distance,
			world.getBuildObjectSystem().getFarthestDetailMeshDistance(camera_position));

		const DynamicArray<PlanetTerrainRenderNode>& terrain_render_nodes =
			getTerrainNodesForRender();
		const auto draw_world_geometry = [&](PlanetTerrainDepthPass depth_pass)
		{
			world.getBuildObjectSystem().renderDetailMeshes(camera_position);
			renderTerrainNodes(terrain_render_nodes, depth_pass);
		};

		if(far_render_distance > camera.far_clip_distance * 1.05)
		{
			float near_clip = camera.far_clip_distance * 0.95f;
			if(closest_planet_surface_distance < std::numeric_limits<double>::max())
				near_clip = glm::min(near_clip, static_cast<float>(glm::max(
					static_cast<double>(camera.near_clip_distance),
					closest_planet_surface_distance * 0.5)));
			const float far_clip = static_cast<float>(glm::min(far_render_distance * 1.02,
				static_cast<double>(std::numeric_limits<float>::max() / 2.0f)));
			setProjectionMatrix(glm::perspective(glm::radians(camera.vertical_fov_degrees),
				screen_ratio_gz, near_clip, far_clip));
			setDepthRange(0.5f, 1.0f);
			draw_world_geometry(PlanetTerrainDepthPass::far);
			camera.frame();
			setDepthRange(0.0f, 0.5f);
			draw_world_geometry(PlanetTerrainDepthPass::near);
		}
		else
		{
			setDepthRange(0.0f, 1.0f);
			draw_world_geometry(PlanetTerrainDepthPass::full);
		}
	}
}
