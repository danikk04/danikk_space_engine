#include <danikk_space_engine/systems/telemetry/telemetry.h>
#include <danikk_space_engine/systems/planet/system.h>

namespace danikk_space_engine
{
	void PlanetTerrainTree::applyRenderSelection(
		const DynamicArray<PlanetTerrainNodeId>& selected_node_ids)
	{
		DSE_WATCH();
		DynamicArray<uint8> target_nodes(nodes.size(), 0);
		DynamicArray<uint8> fallback_nodes(nodes.size(), 0);
		const auto hasDrawableMesh = [](const PlanetTerrainNode& node)
		{
			return node.mesh.mesh != nullptr && node.mesh.material_ranges.size() > 0;
		};
		const auto getNodeDepth = [this](PlanetTerrainNodeId node_id)
		{
			uint8 depth = 0;
			double half_extent = PlanetTerrainTree::initial_root_half_extent;
			while(depth < PlanetTerrainTree::maximum_depth
				&& half_extent > nodes[node_id].half_extent)
			{
				half_extent *= 0.5;
				depth++;
			}
			return depth;
		};
		for(PlanetTerrainNode& node : nodes)
		{
			node.render_active = false;
			node.render_target = false;
			node.render_fallback = false;
		}
		for(PlanetTerrainNodeId node_id : selected_node_ids)
			if(node_id < target_nodes.size())
			{
				target_nodes[node_id] = 1;
				nodes[node_id].render_target = true;
			}

		const auto markFallbacks = [&](auto&& self, PlanetTerrainNodeId node_id,
			PlanetTerrainNodeId drawable_ancestor) -> void
		{
			if(node_id >= nodes.size()) return;
			PlanetTerrainNode& node = nodes[node_id];
			if(hasDrawableMesh(node)) drawable_ancestor = node_id;
			if(target_nodes[node_id] != 0)
			{
				if(!hasDrawableMesh(node) && drawable_ancestor != PlanetTerrainTree::invalid_node
					&& drawable_ancestor != node_id)
				{
					constexpr uint8 maximum_fallback_ancestor_levels = 4;
					const uint8 target_depth = getNodeDepth(node_id);
					const uint8 ancestor_depth = getNodeDepth(drawable_ancestor);
					if(target_depth >= ancestor_depth
						&& target_depth - ancestor_depth <= maximum_fallback_ancestor_levels)
						fallback_nodes[drawable_ancestor] = 1;
				}
				return;
			}
			for(PlanetTerrainNodeId child_id : node.children)
				if(child_id != PlanetTerrainTree::invalid_node)
					self(self, child_id, drawable_ancestor);
		};
		if(root_id < nodes.size())
			markFallbacks(markFallbacks, root_id, PlanetTerrainTree::invalid_node);

		const auto activate = [&](auto&& self, PlanetTerrainNodeId node_id) -> void
		{
			if(node_id >= nodes.size()) return;
			PlanetTerrainNode& node = nodes[node_id];
			if(fallback_nodes[node_id] != 0)
			{
				node.render_active = hasDrawableMesh(node);
				node.render_fallback = node.render_active;
			}
			else if(target_nodes[node_id] != 0)
			{
				node.render_active = hasDrawableMesh(node);
				node.render_fallback = false;
				return;
			}
			for(PlanetTerrainNodeId child_id : node.children)
				if(child_id != PlanetTerrainTree::invalid_node) self(self, child_id);
		};
		if(root_id < nodes.size()) activate(activate, root_id);
	}
}
