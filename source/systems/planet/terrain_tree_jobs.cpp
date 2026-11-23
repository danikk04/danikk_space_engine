#include <algorithm>

#include <danikk_space_engine/systems/telemetry/telemetry.h>
#include <danikk_space_engine/systems/planet/system.h>

namespace danikk_space_engine
{
	void PlanetSystem::updateRenderActivity(PlanetData& planet,
		const glm::dvec3& camera_position, double height, const PlanetSettings& planet_settings,
		double detail_scale, const glm::dvec3& render_window_minimum,
		const glm::dvec3& render_window_maximum,
		DynamicArray<PlanetTerrainNodeId>& selected_node_ids)
	{
		DSE_WATCH();
		PlanetTerrainTree& tree = planet.terrain_tree;
		for(PlanetTerrainNode& node : tree.nodes) node.render_active = false;
		selected_node_ids.clear();
		DynamicArray<PlanetTerrainNodeId> visible_node_ids;

		const auto collect = [&](auto&& self, PlanetTerrainNodeId node_id) -> void
		{
			const PlanetTerrainNode& node = tree.nodes[node_id];
			if(node.half_extent > getTargetHalfExtent(camera_position, detail_scale, node.center))
			{
				if(!node.hasChildren()) return;
				for(uint8 child_index = 0; child_index < 8; ++child_index)
				{
					const PlanetTerrainNodeId child_id = node.children[child_index];
					if(child_id != PlanetTerrainTree::invalid_node && isNodeRelevant(
						render_window_minimum, render_window_maximum, camera_position, height,
						planet_settings, tree.nodes[child_id]))
						self(self, child_id);
				}
				return;
			}
			visible_node_ids.push(node_id);
		};
		if(tree.root_id >= tree.nodes.size()) return;
		collect(collect, tree.root_id);

		const auto getNodeDepth = [&tree](PlanetTerrainNodeId node_id)
		{
			uint8 depth = 0;
			double half_extent = PlanetTerrainTree::initial_root_half_extent;
			while(depth < PlanetTerrainTree::maximum_depth
				&& half_extent > tree.nodes[node_id].half_extent)
			{
				half_extent *= 0.5;
				depth++;
			}
			return depth;
		};
		std::sort(visible_node_ids.begin(), visible_node_ids.end(),
			[&tree, &camera_position, &getNodeDepth](PlanetTerrainNodeId left_id,
				PlanetTerrainNodeId right_id)
			{
				const uint8 left_depth = getNodeDepth(left_id);
				const uint8 right_depth = getNodeDepth(right_id);
				if(left_depth != right_depth) return left_depth > right_depth;
				const glm::dvec3 left_offset = tree.nodes[left_id].center - camera_position;
				const glm::dvec3 right_offset = tree.nodes[right_id].center - camera_position;
				const double left_distance = glm::dot(left_offset, left_offset);
				const double right_distance = glm::dot(right_offset, right_offset);
				return left_distance == right_distance ? left_id < right_id
					: left_distance < right_distance;
			});

		size_t selected_per_level[PlanetTerrainTree::maximum_depth + 1] = {};
		for(PlanetTerrainNodeId node_id : visible_node_ids)
		{
			const uint8 depth = getNodeDepth(node_id);
			if(selected_per_level[depth] >= maximum_visible_meshes_per_level) continue;
			selected_per_level[depth]++;
			selected_node_ids.push(node_id);
			tree.nodes[node_id].render_active = true;
		}
	}
}
