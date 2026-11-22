#include <algorithm>
#include <cmath>
#include <danikk_space_engine/systems/telemetry/telemetry.h>
#include <danikk_space_engine/systems/planet/system.h>

#include "terrain_tree_internal.h"

namespace danikk_space_engine::planet_terrain_tree_internal
{
	double getTargetHalfExtent(const UpdateContext& context, const glm::dvec3& center)
	{
		const double distance = glm::length(context.camera_position - center);
		const double minimum = PlanetTerrainTree::minimum_node_width * 0.5;
		return std::clamp(minimum + context.detail_scale * distance, minimum, 262144.0);
	}

	namespace
	{
		void updateRenderWindow(UpdateContext& context)
		{
			const double root_half_extent = PlanetTerrainTree::initial_root_half_extent;
			const glm::dvec3 root_minimum(-root_half_extent);
			const glm::dvec3 root_maximum(root_half_extent);
			const double camera_distance = glm::length(context.camera_position);
			glm::dvec3 anchor = context.camera_position;
			if(context.height >= 0.0 && camera_distance > 1e-9)
				anchor *= (camera_distance - context.height) / camera_distance;

			const double target_half_extent = getTargetHalfExtent(context, anchor);
			const double depth_value = std::ceil(std::log2(root_half_extent / target_half_extent));
			const uint8 target_depth = static_cast<uint8>(std::clamp(depth_value, 0.0,
				static_cast<double>(PlanetTerrainTree::maximum_depth)));
			const uint8 window_depth = target_depth > 2 ? static_cast<uint8>(target_depth - 2) : 0;
			const int32 cell_count = 1 << window_depth;
			const double cell_width = std::ldexp(root_half_extent * 2.0, -window_depth);
			for(uint8 axis = 0; axis < 3; ++axis)
			{
				int32 center_cell = static_cast<int32>(std::floor(
					(anchor[axis] + root_half_extent) / cell_width));
				center_cell = std::clamp(center_cell, 0, cell_count - 1);
				const int32 first_cell = center_cell - 1;
				const int32 end_cell = center_cell + 2;
				context.render_window_minimum[axis] = glm::max(root_minimum[axis],
					-root_half_extent + first_cell * cell_width);
				context.render_window_maximum[axis] = glm::min(root_maximum[axis],
					-root_half_extent + end_cell * cell_width);
			}
			context.render_window_initialized = true;
		}

		bool intersectsRenderWindow(const UpdateContext& context, const PlanetTerrainNode& node)
		{
			if(!context.render_window_initialized) return false;
			const glm::dvec3 node_minimum = node.center - glm::dvec3(node.half_extent);
			const glm::dvec3 node_maximum = node.center + glm::dvec3(node.half_extent);
			for(uint8 axis = 0; axis < 3; ++axis)
				if(node_maximum[axis] < context.render_window_minimum[axis]
					|| node_minimum[axis] > context.render_window_maximum[axis]) return false;
			return true;
		}

		bool intersectsDirtyRegion(const PlanetTerrainNode& node,
			const PlanetTerrainDirtyRegion& region)
		{
			const glm::dvec3 node_minimum = node.center - glm::dvec3(node.half_extent);
			const glm::dvec3 node_maximum = node.center + glm::dvec3(node.half_extent);
			const glm::dvec3 closest_point = glm::clamp(region.center, node_minimum, node_maximum);
			const glm::dvec3 delta = region.center - closest_point;
			return glm::dot(delta, delta) <= region.radius * region.radius;
		}
	}

	bool isNodeRelevant(const UpdateContext& context, const PlanetTerrainNode& node)
	{
		if(!intersectsRenderWindow(context, node)) return false;
		const double node_radius = node.half_extent * 1.7320508075688772;
		const double center_distance = glm::length(node.center);
		const double minimum_radius = glm::max(0.0, center_distance - node_radius);
		const double maximum_radius = center_distance + node_radius;
		const bool surface_intersection = context.height >= 0.0
			&& maximum_radius >= context.settings.radius - 25000.0
			&& minimum_radius <= context.settings.radius + 15000.0;
		const glm::dvec3 to_node = node.center - context.camera_position;
		const double distance = glm::length(to_node);
		const bool local_cave_intersection = context.height < 0.0
			&& distance <= node_radius + 384.0;
		if(!surface_intersection && !local_cave_intersection) return false;

		if(surface_intersection && context.height > 0.0 && center_distance > 1.0)
		{
			const double camera_distance = glm::length(context.camera_position);
			if(camera_distance > context.settings.radius + 1.0)
			{
				const double horizon_cosine = context.settings.radius / camera_distance;
				const glm::dvec3 camera_direction = context.camera_position / camera_distance;
				const glm::dvec3 node_direction = node.center / center_distance;
				const double angular_radius = std::min(1.0, node_radius / center_distance);
				if(glm::dot(camera_direction, node_direction) + angular_radius < horizon_cosine)
					return false;
			}
		}
		return true;
	}

	bool isSurfaceNode(const UpdateContext& context, const PlanetTerrainNode& node)
	{
		const double radial_extent = node.half_extent * 1.7320508075688772;
		const double center_radius = glm::length(node.center);
		return center_radius + radial_extent >= context.settings.radius - 25000.0
			&& glm::max(0.0, center_radius - radial_extent) <= context.settings.radius + 15000.0;
	}

	bool hasCurrentMesh(const UpdateContext& context, PlanetTerrainNodeId node_id)
	{
		const PlanetTerrainNode& node = context.tree.nodes[node_id];
		return node.mesh_revision == context.planet.terrain_revision
			&& node.mesh_generator == settings.planet_mesh_generator
			&& node.mesh_surface_only == isSurfaceNode(context, node)
			&& node.mesh.mesh != nullptr;
	}

	namespace
	{
		void createRoot(UpdateContext& context)
		{
			const PlanetTerrainNodeId root_id = context.tree.createRoot(
				glm::dvec3(0.0), PlanetTerrainTree::initial_root_half_extent);
			if(root_id == invalid_node) return;
			context.tree.nodes[root_id].generation_id = context.tree.next_node_generation_id++;
			if(context.tree.next_node_generation_id == 0) context.tree.next_node_generation_id++;
		}
	}

	void initialize(UpdateContext& context)
	{
		DSE_WATCH();
		if(context.tree.root_id == invalid_node) createRoot(context);

		if(context.planet.terrain_tree_revision != context.planet.terrain_revision)
		{
			for(PlanetTerrainNode& node : context.tree.nodes)
			{
				node.generation_pending = false;
				bool dirty = context.planet.pending_terrain_dirty_regions.empty();
				for(const PlanetTerrainDirtyRegion& region : context.planet.pending_terrain_dirty_regions)
				{
					if(!intersectsDirtyRegion(node, region)) continue;
					dirty = true;
					break;
				}
				if(dirty)
				{
					node.mesh_revision = 0;
					node.render_active = false;
					node.generation_id = context.tree.next_node_generation_id++;
					if(context.tree.next_node_generation_id == 0) context.tree.next_node_generation_id++;
				}
				else if(node.mesh.mesh != nullptr)
					node.mesh_revision = context.planet.terrain_revision;
			}
			context.planet.pending_terrain_dirty_regions.clear();
			context.planet.terrain_tree_revision = context.planet.terrain_revision;
		}

		if(context.tree.nodes.size() + 8192 <= maximum_tree_nodes) return;
		for(PlanetTerrainNode& node : context.tree.nodes)
		{
			if(node.mesh.mesh == nullptr) continue;
			destroyMeshRenderData(node.mesh);
			context.telemetry.mesh_generation_telemetry.recordMeshDeletion(
				MeshDeletionReason::tree_node_limit);
		}
		context.tree.clear();
		createRoot(context);
	}

	void calculateDetailBudget(UpdateContext& context)
	{
		DSE_WATCH();
		context.detail_scale = context.height < 0.0 ? 0.01 : 0.015;
		updateRenderWindow(context);
		const auto count_visible_target_nodes = [&](auto&& self, const glm::dvec3& center,
			double half_extent, uint8 depth, size_t limit) -> size_t
		{
			PlanetTerrainNode candidate;
			candidate.center = center;
			candidate.half_extent = half_extent;
			if(!isNodeRelevant(context, candidate)) return 0;
			if(half_extent <= getTargetHalfExtent(context, center)
				|| depth >= PlanetTerrainTree::maximum_depth) return 1;
			const double child_half_extent = half_extent * 0.5;
			size_t count = 0;
			for(uint8 child_index = 0; child_index < 8; ++child_index)
			{
				const glm::dvec3 child_center = center + glm::dvec3(
					(child_index & 1) ? child_half_extent : -child_half_extent,
					(child_index & 2) ? child_half_extent : -child_half_extent,
					(child_index & 4) ? child_half_extent : -child_half_extent);
				count += self(self, child_center, child_half_extent,
					static_cast<uint8>(depth + 1), limit - count);
				if(count > limit) return count;
			}
			return count;
		};

		const auto count_nodes = [&]()
		{
			return count_visible_target_nodes(count_visible_target_nodes, glm::dvec3(0.0),
				PlanetTerrainTree::initial_root_half_extent, 0, maximum_visible_nodes);
		};
		context.target_node_count = count_nodes();
		while(context.target_node_count > maximum_visible_nodes
			&& context.detail_scale_adjustments < 32)
		{
			context.detail_scale *= 1.5;
			context.detail_scale_adjustments++;
			updateRenderWindow(context);
			context.target_node_count = count_nodes();
		}
	}

	void refine(UpdateContext& context)
	{
		DSE_WATCH();
		const auto ensure_children = [&context](PlanetTerrainNodeId parent_id)
		{
			PlanetTerrainTree& tree = context.tree;
			if(tree.nodes[parent_id].hasChildren()) return true;
			if(tree.nodes.size() + 8 > maximum_tree_nodes) return false;
			if(!tree.splitNode(parent_id)) return false;
			for(uint8 child_index = 0; child_index < 8; ++child_index)
			{
				const PlanetTerrainNodeId child_id = tree.nodes[parent_id].children[child_index];
				tree.nodes[child_id].generation_id = tree.next_node_generation_id++;
				if(tree.next_node_generation_id == 0) tree.next_node_generation_id++;
			}
			return true;
		};

		const auto visit = [&](auto&& self, PlanetTerrainNodeId node_id, uint8 depth) -> void
		{
			if(node_id >= context.tree.nodes.size()
				|| depth >= PlanetTerrainTree::maximum_depth) return;
			PlanetTerrainNode candidate;
			candidate.center = context.tree.nodes[node_id].center;
			candidate.half_extent = context.tree.nodes[node_id].half_extent;
			if(!isNodeRelevant(context, candidate)
				|| candidate.half_extent <= getTargetHalfExtent(context, candidate.center)) return;
			if(!ensure_children(node_id)) return;
			for(uint8 child_index = 0; child_index < 8; ++child_index)
				self(self, context.tree.nodes[node_id].children[child_index],
					static_cast<uint8>(depth + 1));
		};
		visit(visit, context.tree.root_id, 0);
	}
}
