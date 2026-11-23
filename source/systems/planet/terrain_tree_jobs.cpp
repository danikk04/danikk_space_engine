#include <danikk_space_engine/systems/telemetry/telemetry.h>
#include <danikk_space_engine/systems/planet/system.h>

namespace danikk_space_engine
{
	void PlanetTerrainTree::selectRenderNodes(
		const glm::dvec3& camera_position, double height, const PlanetSettings& planet_settings,
		double detail_scale, const glm::dvec3& render_window_minimum,
		const glm::dvec3& render_window_maximum,
		DynamicArray<PlanetTerrainNodeId>& selected_node_ids)
	{
		DSE_WATCH();
		for(PlanetTerrainNode& node : nodes) node.render_active = false;
		selected_node_ids.clear();
		const auto collect = [&](auto&& self, PlanetTerrainNodeId node_id) -> void
		{
			const PlanetTerrainNode& node = nodes[node_id];
			if(node.half_extent > getTargetHalfExtent(camera_position, detail_scale, node.center))
			{
				if(!node.hasChildren()) return;
				for(uint8 child_index = 0; child_index < 8; ++child_index)
				{
					const PlanetTerrainNodeId child_id = node.children[child_index];
					if(child_id != PlanetTerrainTree::invalid_node && nodes[child_id].isInTerrainRenderRegion(
						render_window_minimum, render_window_maximum, camera_position, height,
						planet_settings))
						self(self, child_id);
				}
				return;
			}
			selected_node_ids.push(node_id);
			nodes[node_id].render_active = true;
		};
		if(root_id >= nodes.size()) return;
		collect(collect, root_id);
	}
}
