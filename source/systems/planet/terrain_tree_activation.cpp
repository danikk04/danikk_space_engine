#include <danikk_space_engine/systems/telemetry/telemetry.h>
#include <danikk_space_engine/systems/planet/system.h>

namespace danikk_space_engine
{
	void PlanetSystem::activateMeshes(PlanetTerrainTree& tree,
		const DynamicArray<PlanetTerrainNodeId>& selected_node_ids) const
	{
		DSE_WATCH();
		DynamicArray<uint8> target_nodes(tree.nodes.size(), 0);
		DynamicArray<uint8> fallback_nodes(tree.nodes.size(), 0);
		for(PlanetTerrainNodeId node_id : selected_node_ids)
			if(node_id < target_nodes.size()) target_nodes[node_id] = 1;

		const auto markFallbacks = [&](auto&& self, PlanetTerrainNodeId node_id,
			PlanetTerrainNodeId drawable_ancestor) -> void
		{
			if(node_id >= tree.nodes.size()) return;
			PlanetTerrainNode& node = tree.nodes[node_id];
			if(node.mesh.mesh != nullptr) drawable_ancestor = node_id;
			if(target_nodes[node_id] != 0)
			{
				if(node.mesh.mesh == nullptr && drawable_ancestor != PlanetTerrainTree::invalid_node
					&& drawable_ancestor != node_id)
					fallback_nodes[drawable_ancestor] = 1;
				return;
			}
			for(PlanetTerrainNodeId child_id : node.children)
				if(child_id != PlanetTerrainTree::invalid_node)
					self(self, child_id, drawable_ancestor);
		};
		if(tree.root_id < tree.nodes.size())
			markFallbacks(markFallbacks, tree.root_id, PlanetTerrainTree::invalid_node);

		const auto activate = [&](auto&& self, PlanetTerrainNodeId node_id) -> void
		{
			if(node_id >= tree.nodes.size()) return;
			PlanetTerrainNode& node = tree.nodes[node_id];
			if(fallback_nodes[node_id] != 0)
			{
				node.render_active = node.mesh.mesh != nullptr;
				return;
			}
			if(target_nodes[node_id] != 0)
			{
				node.render_active = node.mesh.mesh != nullptr;
				return;
			}
			for(PlanetTerrainNodeId child_id : node.children)
				if(child_id != PlanetTerrainTree::invalid_node) self(self, child_id);
		};
		if(tree.root_id < tree.nodes.size()) activate(activate, tree.root_id);
	}
}
