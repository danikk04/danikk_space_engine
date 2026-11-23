#include <algorithm>
#include <cmath>
#include <danikk_space_engine/systems/telemetry/telemetry.h>
#include <danikk_space_engine/systems/planet/system.h>
#include <danikk_space_engine/world.h>

namespace danikk_space_engine
{
	double PlanetTerrainTree::getTargetHalfExtent(const glm::dvec3& camera_position,
		double detail_scale, const glm::dvec3& center)
	{
		const double distance = glm::length(camera_position - center);
		const double minimum = PlanetTerrainTree::minimum_node_width * 0.5;
		return std::clamp(minimum + detail_scale * distance, minimum, 262144.0);
	}

	void PlanetTerrainTree::updateRenderWindow(const glm::dvec3& camera_position, double height,
		const PlanetSettings& settings, glm::dvec3& render_window_minimum,
		glm::dvec3& render_window_maximum)
	{
		const double root_half_extent = PlanetTerrainTree::initial_root_half_extent;
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

	bool PlanetTerrainNode::isRelevantTo(const glm::dvec3& render_window_minimum,
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

	void PlanetSystem::synchronizeTerrainTree(PlanetData& planet)
	{
		DSE_WATCH();
		PlanetTerrainTree& tree = planet.terrain_tree;
		if(tree.root_id == PlanetTerrainTree::invalid_node) tree.createRoot();

		if(planet.terrain_tree_revision != planet.terrain_revision)
		{
			for(PlanetTerrainNode& node : tree.nodes)
			{
				node.generation_pending = false;
				node.pending_terrain_revision = 0;
				node.pending_node_generation_id = 0;
				bool dirty = planet.pending_terrain_dirty_regions.empty();
				for(const PlanetTerrainDirtyRegion& region : planet.pending_terrain_dirty_regions)
				{
					if(!node.intersectsSphere(region.center, region.radius)) continue;
					dirty = true;
					break;
				}
				if(dirty)
				{
					node.mesh_revision = 0;
					node.mesh_generated = false;
					node.render_active = false;
					node.generation_id = tree.next_node_generation_id++;
					if(tree.next_node_generation_id == 0) tree.next_node_generation_id++;
				}
				else if(node.mesh_generated)
					node.mesh_revision = planet.terrain_revision;
			}
			planet.pending_terrain_dirty_regions.clear();
			planet.terrain_tree_revision = planet.terrain_revision;
		}

		if(tree.nodes.size() + 8192 <= PlanetTerrainTree::maximum_node_count) return;
		for(PlanetTerrainNode& node : tree.nodes)
		{
			if(node.mesh.mesh == nullptr) continue;
			destroyMeshRenderData(node.mesh);
			world.getTelemetrySystem().mesh_generation_telemetry.recordMeshDeletion(
				MeshDeletionReason::tree_node_limit);
		}
		tree.clear();
		tree.createRoot();
	}

	void PlanetTerrainTree::calculateDetailScale(const glm::dvec3& camera_position, double height,
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
		const auto count_nodes = [&]()
		{
			size_t nodes_per_level[PlanetTerrainTree::maximum_depth + 1] = {};
			const auto count_visible_target_nodes = [&](auto&& self, const glm::dvec3& center,
				double half_extent, uint8 depth) -> size_t
			{
				PlanetTerrainNode candidate;
				candidate.center = center;
				candidate.half_extent = half_extent;
				if(!candidate.isRelevantTo(render_window_minimum, render_window_maximum,
					camera_position, height, settings)) return 0;
				if(half_extent <= getTargetHalfExtent(camera_position, detail_scale, center)
					|| depth >= PlanetTerrainTree::maximum_depth)
				{
					if(++nodes_per_level[depth] > maximum_visible_meshes_per_level)
						return maximum_visible_nodes + 1;
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
					count += self(self, child_center, child_half_extent,
						static_cast<uint8>(depth + 1));
					if(count > maximum_visible_nodes) return count;
				}
				return count;
			};
			size_t total = count_visible_target_nodes(count_visible_target_nodes,
				glm::dvec3(0.0), PlanetTerrainTree::initial_root_half_extent, 0);
			if(total > maximum_visible_nodes) return total;
			total = 0;
			for(uint8 depth = 0; depth <= PlanetTerrainTree::maximum_depth; ++depth)
				total += nodes_per_level[depth];
			return total;
		};
		target_node_count = count_nodes();
		while(target_node_count > maximum_visible_nodes && detail_scale_adjustments < 32)
		{
			detail_scale *= 1.5;
			detail_scale_adjustments++;
			updateRenderWindow(camera_position, height, settings,
				render_window_minimum, render_window_maximum);
			target_node_count = count_nodes();
		}
	}

	void PlanetTerrainTree::refine(const glm::dvec3& camera_position,
		double height, const PlanetSettings& settings, double detail_scale,
		const glm::dvec3& render_window_minimum, const glm::dvec3& render_window_maximum)
	{
		DSE_WATCH();
		const auto visit = [&](auto&& self, PlanetTerrainNodeId node_id, uint8 depth) -> void
		{
			if(node_id >= nodes.size()
				|| depth >= PlanetTerrainTree::maximum_depth) return;
			PlanetTerrainNode candidate;
			candidate.center = nodes[node_id].center;
			candidate.half_extent = nodes[node_id].half_extent;
			if(!candidate.isRelevantTo(render_window_minimum, render_window_maximum,
				camera_position, height, settings)
				|| candidate.half_extent <= PlanetTerrainTree::getTargetHalfExtent(camera_position,
					detail_scale, candidate.center)) return;
			if(!ensureChildren(node_id)) return;
			for(uint8 child_index = 0; child_index < 8; ++child_index)
				self(self, nodes[node_id].children[child_index],
					static_cast<uint8>(depth + 1));
		};
		if(root_id < nodes.size()) visit(visit, root_id, 0);
	}
}
