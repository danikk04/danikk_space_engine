#include <danikk_space_engine/systems/telemetry/telemetry.h>
#include <danikk_space_engine/systems/planet/system.h>

namespace danikk_space_engine
{
	void PlanetSystem::activateMeshes(PlanetTerrainTree& tree, const glm::dvec3& camera_position,
		double height, const PlanetSettings& settings, double detail_scale,
		const glm::dvec3& render_window_minimum, const glm::dvec3& render_window_maximum) const
	{
		DSE_WATCH();
		const auto activate = [&](auto&& self, PlanetTerrainNodeId node_id) -> void
		{
			PlanetTerrainNode& node = tree.nodes[node_id];
			if(node.half_extent <= getTargetHalfExtent(camera_position, detail_scale, node.center))
			{
				// Keep this node's previous mesh visible while its replacement is queued
				// or generated. A parent node is still not used as a substitute for
				// missing detail because traversal only reaches target-resolution nodes.
				if(node.mesh.mesh != nullptr) node.render_active = true;
				return;
			}
			if(!node.hasChildren()) return;

			// Do not substitute a coarser parent mesh while the requested detail is
			// still generating. Missing fine terrain should remain unrendered.
			for(uint8 child_index = 0; child_index < 8; ++child_index)
			{
				const PlanetTerrainNodeId child_id = node.children[child_index];
				if(child_id != PlanetTerrainTree::invalid_node && isNodeRelevant(render_window_minimum,
					render_window_maximum, camera_position, height, settings, tree.nodes[child_id]))
					self(self, child_id);
			}
		};
		if(tree.root_id < tree.nodes.size()) activate(activate, tree.root_id);
	}
}
