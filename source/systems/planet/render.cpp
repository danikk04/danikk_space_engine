#include <cmath>

#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>

#include <danikk_space_engine/systems/telemetry/telemetry.h>
#include <danikk_space_engine/systems/planet/system.h>
#include <danikk_space_engine/world.h>

#include <danikk_space_engine/systems/camera/camera.h>

namespace danikk_space_engine
{
	namespace
	{
		/** @brief Temporarily outlines fallback geometry where the detail stencil is set. */
		constexpr bool show_terrain_stencil_debug_overlay = false;

		/** @brief Clears stencil pixels inside the projected bounds owned by a detail node. */
		void markProjectedNodeCoverage(const PlanetData& planet, const PlanetTerrainNode& node,
			const glm::dvec3& camera_position, const glm::dmat4& view_projection,
			const GLint* viewport)
		{
			double minimum_x = 1.0;
			double minimum_y = 1.0;
			double maximum_x = -1.0;
			double maximum_y = -1.0;
			bool has_front_corner = false;
			bool crosses_camera_plane = false;
			for(uint8 corner = 0; corner < 8; ++corner)
			{
				const glm::dvec3 local_corner = node.center + glm::dvec3(
					(corner & 1) ? node.half_extent : -node.half_extent,
					(corner & 2) ? node.half_extent : -node.half_extent,
					(corner & 4) ? node.half_extent : -node.half_extent);
				const glm::dvec3 relative_corner = planet.position
					+ planet.rotation * local_corner - camera_position;
				const glm::dvec4 clip = view_projection * glm::dvec4(relative_corner, 1.0);
				if(clip.w <= 1e-8)
				{
					crosses_camera_plane = true;
					continue;
				}
				const double inverse_w = 1.0 / clip.w;
				const double x = clip.x * inverse_w;
				const double y = clip.y * inverse_w;
				minimum_x = glm::min(minimum_x, x);
				minimum_y = glm::min(minimum_y, y);
				maximum_x = glm::max(maximum_x, x);
				maximum_y = glm::max(maximum_y, y);
				has_front_corner = true;
			}

			if(crosses_camera_plane || !has_front_corner) return;
			if(maximum_x < -1.0 || minimum_x > 1.0 || maximum_y < -1.0 || minimum_y > 1.0) return;

			const GLint x0 = viewport[0] + static_cast<GLint>(std::floor((glm::clamp(minimum_x, -1.0, 1.0) + 1.0) * 0.5 * viewport[2]));
			const GLint y0 = viewport[1] + static_cast<GLint>(std::floor((glm::clamp(minimum_y, -1.0, 1.0) + 1.0) * 0.5 * viewport[3]));
			const GLint x1 = viewport[0] + static_cast<GLint>(std::ceil((glm::clamp(maximum_x, -1.0, 1.0) + 1.0) * 0.5 * viewport[2]));
			const GLint y1 = viewport[1] + static_cast<GLint>(std::ceil((glm::clamp(maximum_y, -1.0, 1.0) + 1.0) * 0.5 * viewport[3]));
			if(x1 <= x0 || y1 <= y0) return;
			glScissor(x0, y0, x1 - x0, y1 - y0);
			glClear(GL_STENCIL_BUFFER_BIT);
		}

		/** @brief Draws one node mesh using its planet-relative transform. */
		void drawPlanetNodeMesh(const PlanetData& planet, const PlanetTerrainNode& node,
			const glm::dvec3& camera_position)
		{
			const glm::dvec3 relative_position = planet.position
				+ planet.rotation * node.mesh_position - camera_position;
			mat4 body_rotation(1.0f);
			body_rotation[0] = vec4(vec3(planet.rotation * glm::dvec3(1.0, 0.0, 0.0)), 0.0f);
			body_rotation[1] = vec4(vec3(planet.rotation * glm::dvec3(0.0, 1.0, 0.0)), 0.0f);
			body_rotation[2] = vec4(vec3(planet.rotation * glm::dvec3(0.0, 0.0, 1.0)), 0.0f);
			drawMeshRenderData(node.mesh,
				glm::translate(mat4(1.0f), vec3(relative_position)) * body_rotation);
		}
	}

	/** @brief Draws the supplied detail and fallback nodes in the current depth pass. */
	void PlanetSystem::renderTerrainNodes(
		const DynamicArray<PlanetTerrainRenderNode>& render_nodes,
		PlanetTerrainDepthPass depth_pass)
	{
		DSE_WATCH();
		for(size_t index = 0; index < terrain_gpu_query_count; ++index)
		{
			if(!terrain_gpu_query_pending[index]) continue;
			GLint available = GL_FALSE;
			glGetQueryObjectiv(terrain_gpu_query_ids[index], GL_QUERY_RESULT_AVAILABLE, &available);
			if(available == GL_FALSE) continue;
			GLuint64 elapsed_nanoseconds = 0;
			glGetQueryObjectui64v(terrain_gpu_query_ids[index], GL_QUERY_RESULT,
				&elapsed_nanoseconds);
			last_terrain_gpu_time_milliseconds = static_cast<double>(elapsed_nanoseconds) / 1000000.0;
			has_last_terrain_gpu_time = true;
			terrain_gpu_query_pending[index] = false;
		}

		size_t active_query_index = terrain_gpu_query_count;
		for(size_t offset = 0; offset < terrain_gpu_query_count; ++offset)
		{
			const size_t index = (terrain_gpu_query_write_index + offset) % terrain_gpu_query_count;
			if(terrain_gpu_query_pending[index] || terrain_gpu_query_ids[index] == 0) continue;
			active_query_index = index;
			break;
		}
		if(active_query_index < terrain_gpu_query_count)
			glBeginQuery(GL_TIME_ELAPSED, terrain_gpu_query_ids[active_query_index]);

		const CameraSystem& camera = world.getCameraSystem();
		const glm::dvec3 camera_position = camera.getWorldPosition();
		const auto belongsToDepthPass = [&](const PlanetData& planet, const PlanetTerrainNode& node)
		{
			if(depth_pass == PlanetTerrainDepthPass::full) return true;
			const glm::dvec3 node_world_center = planet.position + planet.rotation * node.center;
			const double center_distance = glm::length(node_world_center - camera_position);
			const double bounding_radius = node.half_extent * 1.7320508075688772;
			if(depth_pass == PlanetTerrainDepthPass::near)
				return center_distance - bounding_radius <= camera.far_clip_distance;
			return center_distance + bounding_radius >= camera.far_clip_distance;
		};
		GLint viewport[4] = {};
		GLint stencil_bits = 0;
		glGetIntegerv(GL_VIEWPORT, viewport);
		glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_STENCIL,
			GL_FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE, &stencil_bits);
		const bool has_stencil = stencil_bits > 0;

		if(has_stencil)
		{
			const double near_clip = glm::max(static_cast<double>(camera.near_clip_distance), 0.01);
			const double far_clip = glm::max(static_cast<double>(camera.far_clip_distance), near_clip + 1.0);
			const glm::dmat4 projection = glm::perspective(glm::radians(
				static_cast<double>(camera.vertical_fov_degrees)),
				static_cast<double>(viewport[3] > 0 ? viewport[2] : 1)
					/ static_cast<double>(viewport[3] > 0 ? viewport[3] : 1), near_clip, far_clip);
			const glm::dmat4 view = glm::lookAt(glm::dvec3(0.0), glm::dvec3(camera.getFront()),
				glm::dvec3(camera.getUp()));
			const glm::dmat4 view_projection = projection * view;
			glStencilMask(0xFF);
			glClearStencil(0);
			glClear(GL_STENCIL_BUFFER_BIT);
			glEnable(GL_SCISSOR_TEST);
			glClearStencil(1);
			for(const PlanetTerrainRenderNode& render_node : render_nodes)
			{
				if(render_node.fallback) continue;
				PlanetData* planet = findPlanet(render_node.planet_id);
				if(planet == nullptr || render_node.node_id >= planet->terrain_tree.nodes.size()) continue;
				const PlanetTerrainNode& node = planet->terrain_tree.nodes[render_node.node_id];
				if(!belongsToDepthPass(*planet, node)) continue;
				const bool has_drawable_mesh = node.mesh.mesh != nullptr
					&& node.mesh.material_ranges.size() > 0;
				const bool current_empty_result = node.mesh_generated
					&& node.mesh_revision == planet->terrain_revision
					&& !has_drawable_mesh;
				if(has_drawable_mesh || current_empty_result)
					markProjectedNodeCoverage(*planet, node, camera_position,
						view_projection, viewport);
			}
			glClearStencil(0);
			glDisable(GL_SCISSOR_TEST);
		}

		glDisable(GL_STENCIL_TEST);
		for(const PlanetTerrainRenderNode& render_node : render_nodes)
		{
			if(render_node.fallback) continue;
			PlanetData* planet = findPlanet(render_node.planet_id);
			if(planet == nullptr || render_node.node_id >= planet->terrain_tree.nodes.size()) continue;
			const PlanetTerrainNode& node = planet->terrain_tree.nodes[render_node.node_id];
			if(!belongsToDepthPass(*planet, node)) continue;
			if(node.mesh.mesh != nullptr) drawPlanetNodeMesh(*planet, node, camera_position);
		}

		if(has_stencil)
		{
			glStencilMask(0x00);
			glStencilFunc(GL_EQUAL, 0, 0xFF);
			glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
			glEnable(GL_STENCIL_TEST);
		}
		for(const PlanetTerrainRenderNode& render_node : render_nodes)
		{
			if(!render_node.fallback) continue;
			PlanetData* planet = findPlanet(render_node.planet_id);
			if(planet == nullptr || render_node.node_id >= planet->terrain_tree.nodes.size()) continue;
			const PlanetTerrainNode& node = planet->terrain_tree.nodes[render_node.node_id];
			if(!belongsToDepthPass(*planet, node)) continue;
			if(node.mesh.mesh != nullptr) drawPlanetNodeMesh(*planet, node, camera_position);
		}
		if constexpr(show_terrain_stencil_debug_overlay)
		{
			if(has_stencil)
			{
				GLboolean old_depth_write = GL_TRUE;
				GLint old_polygon_mode[2] = {GL_FILL, GL_FILL};
				const GLboolean old_depth_test = glIsEnabled(GL_DEPTH_TEST);
				glGetBooleanv(GL_DEPTH_WRITEMASK, &old_depth_write);
				glGetIntegerv(GL_POLYGON_MODE, old_polygon_mode);
				glStencilFunc(GL_NOTEQUAL, 0, 0xFF);
				glDepthMask(GL_FALSE);
				glDisable(GL_DEPTH_TEST);
				glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
				for(const PlanetTerrainRenderNode& render_node : render_nodes)
				{
					if(!render_node.fallback) continue;
					PlanetData* planet = findPlanet(render_node.planet_id);
					if(planet == nullptr || render_node.node_id >= planet->terrain_tree.nodes.size()) continue;
					const PlanetTerrainNode& node = planet->terrain_tree.nodes[render_node.node_id];
					if(!belongsToDepthPass(*planet, node)) continue;
					if(node.mesh.mesh != nullptr) drawPlanetNodeMesh(*planet, node, camera_position);
				}
				glPolygonMode(GL_FRONT_AND_BACK, static_cast<GLenum>(old_polygon_mode[0]));
				if(old_depth_test) glEnable(GL_DEPTH_TEST);
				else glDisable(GL_DEPTH_TEST);
				glDepthMask(old_depth_write);
			}
		}
		if(has_stencil)
		{
			glDisable(GL_STENCIL_TEST);
			glStencilMask(0xFF);
		}
		if(active_query_index < terrain_gpu_query_count)
		{
			glEndQuery(GL_TIME_ELAPSED);
			terrain_gpu_query_pending[active_query_index] = true;
			terrain_gpu_query_write_index = (active_query_index + 1) % terrain_gpu_query_count;
		}
	}

	bool PlanetSystem::getLastTerrainGpuTimeMilliseconds(double& milliseconds) const
	{
		milliseconds = last_terrain_gpu_time_milliseconds;
		return has_last_terrain_gpu_time;
	}
}
