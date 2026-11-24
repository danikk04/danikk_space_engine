#include <algorithm>
#include <cmath>
#include <danikk_space_engine/systems/telemetry/telemetry.h>
#include <danikk_space_engine/systems/planet/system.h>
#include <danikk_space_engine/world.h>

namespace danikk_space_engine
{
	double PlanetTerrainTree::getTargetHalfExtent(const glm::dvec3& camera_position,
		double detail_scale, const glm::dvec3& center) const
	{
		const double distance = glm::length(camera_position - center);
		const double minimum = minimum_node_width * 0.5;
		return std::clamp(minimum + detail_scale * distance, minimum, 262144.0);
	}

	void PlanetTerrainTree::updateRenderWindow(const glm::dvec3& camera_position, double height,
		const PlanetSettings& settings, glm::dvec3& render_window_minimum,
		glm::dvec3& render_window_maximum)
	{
		const double root_half_extent = initial_root_half_extent;
		const glm::dvec3 root_minimum(-root_half_extent);
		const glm::dvec3 root_maximum(root_half_extent);
		glm::dvec3 anchor = camera_position;
		double half_window_extent = 384.0;
		if(height >= 0.0)
		{
			const double camera_distance = glm::length(camera_position);
			if(camera_distance > 1e-9)
				anchor *= (camera_distance - height) / camera_distance;
			const double horizon_distance = std::sqrt(glm::max(0.0,
				(camera_distance - settings.radius) * (camera_distance + settings.radius)));
			half_window_extent = glm::max(192.0, horizon_distance + 192.0);
		}
		for(uint8 axis = 0; axis < 3; ++axis)
		{
			render_window_minimum[axis] = glm::max(root_minimum[axis],
				anchor[axis] - half_window_extent);
			render_window_maximum[axis] = glm::min(root_maximum[axis],
				anchor[axis] + half_window_extent);
		}
	}

	bool PlanetTerrainNode::isInTerrainRenderRegion(const glm::dvec3& render_window_minimum,
		const glm::dvec3& render_window_maximum, const glm::dvec3& camera_position,
		double height, const PlanetSettings& settings) const
	{
		if(!intersectsBounds(render_window_minimum, render_window_maximum)) return false;
		const double node_radius = half_extent * 1.7320508075688772;
		const double center_distance = glm::length(center);
		const double minimum_radius = glm::max(0.0, center_distance - node_radius);
		const double maximum_radius = center_distance + node_radius;
		const bool surface_intersection = height >= 0.0
			&& maximum_radius >= settings.radius - 25000.0
			&& minimum_radius <= settings.radius + 15000.0;
		const glm::dvec3 to_node = center - camera_position;
		const double distance = glm::length(to_node);
		const bool local_cave_intersection = height < 0.0
			&& distance <= node_radius + 384.0;
		if(!surface_intersection && !local_cave_intersection) return false;

		if(surface_intersection && height > 0.0 && center_distance > 1.0)
		{
			const double camera_distance = glm::length(camera_position);
			if(camera_distance > settings.radius + 1.0)
			{
				const double horizon_cosine = settings.radius / camera_distance;
				const glm::dvec3 camera_direction = camera_position / camera_distance;
				const glm::dvec3 node_direction = center / center_distance;
				const double angular_radius = std::min(1.0, node_radius / center_distance);
				if(glm::dot(camera_direction, node_direction) + angular_radius < horizon_cosine)
					return false;
			}
		}
		return true;
	}

	bool PlanetTerrainTree::isSurfaceOnly(const PlanetTerrainNode& node,
		double planet_radius) const
	{
		return node.intersectsSurfaceBand(planet_radius, 25000.0, 15000.0);
	}

	bool PlanetTerrainTree::isInRenderCoverage(const PlanetTerrainNode& node,
		const glm::dvec3& anchor, uint8 target_depth) const
	{
		if(node.depth > target_depth || target_depth - node.depth > 5) return false;
		const double node_width = node.half_extent * 2.0;
		int32 minimum_index[3];
		int32 node_index[3];
		for(uint8 axis = 0; axis < 3; ++axis)
		{
			const double anchor_grid_position = (anchor[axis] + initial_root_half_extent) / node_width;
			const double node_grid_position = (node.center[axis] + initial_root_half_extent) / node_width;
			minimum_index[axis] = static_cast<int32>(std::floor(anchor_grid_position)) - 4;
			node_index[axis] = static_cast<int32>(std::floor(node_grid_position));
			if(node_index[axis] < minimum_index[axis]
				|| node_index[axis] >= minimum_index[axis] + 8) return false;
		}

		// The finest selected level owns the complete 8x8x8 cube. Each coarser
		// level leaves its central 4x4x4 cells to the immediately finer level.
		if(node.depth == target_depth) return true;
		for(uint8 axis = 0; axis < 3; ++axis)
			if(node_index[axis] < minimum_index[axis] + 2
				|| node_index[axis] >= minimum_index[axis] + 6) return true;
		return false;
	}

	void PlanetTerrainTree::calculateStreamingDetailScale(const glm::dvec3& camera_position, double height,
		const PlanetSettings& settings, glm::dvec3& render_window_minimum,
		glm::dvec3& render_window_maximum, double& detail_scale,
		size_t& target_node_count, size_t& detail_scale_adjustments)
	{
		DSE_WATCH();
		detail_scale = height < 0.0 ? 0.01 : 0.015;
		target_node_count = 0;
		detail_scale_adjustments = 0;
		updateRenderWindow(camera_position, height, settings,
			render_window_minimum, render_window_maximum);
		target_node_count = 0;
		countTargetNodesRecursive(glm::dvec3(0.0), initial_root_half_extent, 0,
			camera_position, height, settings, detail_scale, render_window_minimum,
			render_window_maximum, target_node_count);
		while(target_node_count > maximum_target_nodes && detail_scale_adjustments < 32)
		{
			detail_scale *= 1.5;
			detail_scale_adjustments++;
			updateRenderWindow(camera_position, height, settings,
				render_window_minimum, render_window_maximum);
			target_node_count = 0;
			countTargetNodesRecursive(glm::dvec3(0.0), initial_root_half_extent, 0,
				camera_position, height, settings, detail_scale, render_window_minimum,
				render_window_maximum, target_node_count);
		}
	}

	size_t PlanetTerrainTree::countTargetNodesRecursive(const glm::dvec3& center,
		double half_extent, uint8 depth, const glm::dvec3& camera_position,
		double height, const PlanetSettings& settings, double detail_scale,
		const glm::dvec3& render_window_minimum,
		const glm::dvec3& render_window_maximum, size_t& total) const
	{
		PlanetTerrainNode candidate;
		candidate.center = center;
		candidate.half_extent = half_extent;
		if(!candidate.isInTerrainRenderRegion(render_window_minimum, render_window_maximum,
			camera_position, height, settings)) return 0;
		if(half_extent <= getTargetHalfExtent(camera_position, detail_scale, center)
			|| depth >= maximum_depth)
		{
			if(++total > maximum_target_nodes) return maximum_target_nodes + 1;
			return 1;
		}

		const double child_half_extent = half_extent * 0.5;
		size_t count = 0;
		for(uint8 child_index = 0; child_index < 8; ++child_index)
		{
			const glm::dvec3 child_center = center + glm::dvec3(
				(child_index & 1) ? child_half_extent : -child_half_extent,
				(child_index & 2) ? child_half_extent : -child_half_extent,
				(child_index & 4) ? child_half_extent : -child_half_extent);
			count += countTargetNodesRecursive(child_center, child_half_extent,
				static_cast<uint8>(depth + 1), camera_position, height, settings,
				detail_scale, render_window_minimum, render_window_maximum, total);
			if(count > maximum_target_nodes) return count;
		}
		return count;
	}

	void PlanetTerrainTree::refine(const glm::dvec3& camera_position,
		double height, const PlanetSettings& settings, double detail_scale,
		const glm::dvec3& render_window_minimum, const glm::dvec3& render_window_maximum)
	{
		DSE_WATCH();
		if(root_id < nodes.size())
			refineNode(root_id, 0, camera_position, height, settings, detail_scale,
				render_window_minimum, render_window_maximum);
	}

	void PlanetTerrainTree::refineNode(PlanetTerrainNodeId node_id, uint8 depth,
		const glm::dvec3& camera_position, double height,
		const PlanetSettings& settings, double detail_scale,
		const glm::dvec3& render_window_minimum,
		const glm::dvec3& render_window_maximum)
	{
		if(node_id >= nodes.size() || depth >= maximum_depth) return;
		PlanetTerrainNode candidate;
		candidate.center = nodes[node_id].center;
		candidate.half_extent = nodes[node_id].half_extent;
		if(!candidate.isInTerrainRenderRegion(render_window_minimum, render_window_maximum,
			camera_position, height, settings)
			|| candidate.half_extent <= getTargetHalfExtent(camera_position,
				detail_scale, candidate.center)) return;
		if(!ensureChildren(node_id)) return;
		for(uint8 child_index = 0; child_index < 8; ++child_index)
			refineNode(nodes[node_id].children[child_index], static_cast<uint8>(depth + 1),
				camera_position, height, settings, detail_scale,
				render_window_minimum, render_window_maximum);
	}
}
