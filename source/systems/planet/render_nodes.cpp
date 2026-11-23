#include <algorithm>

#include <danikk_space_engine/systems/telemetry/telemetry.h>
#include <danikk_space_engine/systems/planet/system.h>
#include <danikk_space_engine/world.h>

#include <danikk_space_engine/systems/camera/camera.h>

namespace danikk_space_engine
{
	namespace
	{
		/** @brief Collects visible target nodes in detail-first order with the per-level cap applied. */
		void collectTargetNodes(PlanetData& planet, const PlanetSettings& settings,
			DynamicArray<PlanetTerrainNodeId>& selected_node_ids)
		{
			PlanetTerrainTree& tree = planet.terrain_tree;
			selected_node_ids.clear();
			if(tree.root_id >= tree.nodes.size()) return;
			DynamicArray<PlanetTerrainNodeId> visible_node_ids;
			const auto collect = [&](auto&& self, PlanetTerrainNodeId node_id) -> void
			{
				const PlanetTerrainNode& node = tree.nodes[node_id];
				if(node.half_extent > PlanetTerrainTree::getTargetHalfExtent(
					planet.streaming_camera_position, planet.streaming_detail_scale, node.center))
				{
					if(!node.hasChildren()) return;
					for(PlanetTerrainNodeId child_id : node.children)
					{
						if(child_id == PlanetTerrainTree::invalid_node) continue;
						if(tree.nodes[child_id].isInTerrainRenderRegion(
							planet.render_window_minimum, planet.render_window_maximum,
							planet.streaming_camera_position, planet.streaming_height, settings))
							self(self, child_id);
					}
					return;
				}
				visible_node_ids.push(node_id);
			};
			collect(collect, tree.root_id);

			std::sort(visible_node_ids.begin(), visible_node_ids.end(),
				[&tree, &planet](PlanetTerrainNodeId left_id, PlanetTerrainNodeId right_id)
				{
					const PlanetTerrainNode& left = tree.nodes[left_id];
					const PlanetTerrainNode& right = tree.nodes[right_id];
					const uint8 left_depth = left.depth;
					const uint8 right_depth = right.depth;
					if(left_depth != right_depth) return left_depth > right_depth;
					const glm::dvec3 left_offset = left.center - planet.streaming_camera_position;
					const glm::dvec3 right_offset = right.center - planet.streaming_camera_position;
					const double left_distance = glm::dot(left_offset, left_offset);
					const double right_distance = glm::dot(right_offset, right_offset);
					return left_distance == right_distance ? left_id < right_id
						: left_distance < right_distance;
				});

			size_t selected_per_level[PlanetTerrainTree::maximum_depth + 1] = {};
			for(PlanetTerrainNodeId node_id : visible_node_ids)
			{
				const uint8 depth = tree.nodes[node_id].depth;
				if(selected_per_level[depth] >= PlanetTerrainTree::maximum_visible_meshes_per_level) continue;
				selected_per_level[depth]++;
				selected_node_ids.push(node_id);
			}
		}
	}

	const DynamicArray<PlanetTerrainRenderNode>& PlanetSystem::getTerrainNodesForRender()
	{
		DSE_WATCH();
		// The previous output list is also the exact set of nodes whose transient
		// render flags need clearing. Avoid touching every node in every tree.
		for(const PlanetTerrainRenderNode& render_node : terrain_render_nodes)
		{
			PlanetData* planet = findPlanet(render_node.planet_id);
			if(planet == nullptr || render_node.node_id >= planet->terrain_tree.nodes.size()) continue;
			PlanetTerrainNode& node = planet->terrain_tree.nodes[render_node.node_id];
			node.render_active = false;
			node.render_fallback = false;
		}
		terrain_render_nodes.clear();
		const bool has_camera = world.getCameraSystem().isActive();
		for(PlanetData& planet : planets)
		{
			PlanetTerrainTree& tree = planet.terrain_tree;
			if(!planet.terrain_streaming_active || tree.root_id >= tree.nodes.size()) continue;

			DynamicArray<PlanetTerrainNodeId> selected_node_ids;
			collectTargetNodes(planet, getPlanetSettings(planet.settings_id), selected_node_ids);

			const auto hasDrawableMesh = [](const PlanetTerrainNode& node)
			{
				return node.mesh.mesh != nullptr && node.mesh.material_ranges.size() > 0;
			};
			constexpr uint8 maximum_fallback_ancestor_levels = 4;
			for(PlanetTerrainNodeId node_id : selected_node_ids)
			{
				if(node_id >= tree.nodes.size()) continue;
				PlanetTerrainNode& node = tree.nodes[node_id];
				if(hasDrawableMesh(node)) node.render_active = true;
				terrain_render_nodes.push(PlanetTerrainRenderNode{planet.id, node_id, false});

				if(hasDrawableMesh(node)) continue;
				const uint8 target_depth = node.depth;
				PlanetTerrainNodeId ancestor_id = node.parent_id;
				while(ancestor_id != PlanetTerrainTree::invalid_node && ancestor_id < tree.nodes.size())
				{
					PlanetTerrainNode& ancestor = tree.nodes[ancestor_id];
					if(hasDrawableMesh(ancestor))
					{
						const uint8 ancestor_depth = ancestor.depth;
						if(target_depth >= ancestor_depth
							&& target_depth - ancestor_depth <= maximum_fallback_ancestor_levels
							&& !ancestor.render_fallback)
						{
							ancestor.render_fallback = true;
							ancestor.render_active = true;
							terrain_render_nodes.push(PlanetTerrainRenderNode{
								planet.id, ancestor_id, true});
						}
						break;
					}
					ancestor_id = ancestor.parent_id;
				}
			}
			planet.recordDiagnostics(world.getTelemetrySystem().mesh_generation_telemetry,
				planet.streaming_target_node_count, planet.streaming_detail_scale_adjustments,
				planet.streaming_detail_scale);
		}

		if(has_camera)
		{
			for(PlanetData& planet : planets)
			{
				if(!planet.terrain_streaming_active) continue;
				const glm::dvec3 camera_position = glm::conjugate(planet.rotation)
					* (world.getCameraSystem().getWorldPosition() - planet.position);
				evictCachedMeshes(planet.terrain_tree, camera_position);
			}
		}
		return terrain_render_nodes;
	}
}
