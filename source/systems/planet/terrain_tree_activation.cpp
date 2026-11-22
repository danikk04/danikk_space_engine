#include <danikk_space_engine/systems/telemetry/telemetry.h>
#include <danikk_space_engine/systems/planet/system.h>

#include "terrain_tree_internal.h"

namespace danikk_space_engine::planet_terrain_tree_internal
{
	void activateMeshes(UpdateContext& context)
	{
		DSE_WATCH();
		const auto activate = [&](auto&& self, PlanetTerrainNodeId node_id) -> void
		{
			PlanetTerrainNode& node = context.tree.nodes[node_id];
			if(node.half_extent <= getTargetHalfExtent(context, node.center))
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
				if(child_id != invalid_node && isNodeRelevant(context, context.tree.nodes[child_id]))
					self(self, child_id);
			}
		};
		activate(activate, context.tree.root_id);
	}
}
