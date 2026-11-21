#include <danikk_space_engine/systems/telemetry/telemetry.h>
#include <danikk_space_engine/systems/planet/system.h>

#include "terrain_tree_internal.h"

namespace danikk_space_engine::planet_terrain_tree_internal
{
	void activateMeshes(UpdateContext& context)
	{
		DSE_WATCH();
		const auto children_ready = [&](PlanetTerrainNodeId node_id)
		{
			const PlanetTerrainNode& node = context.tree.nodes[node_id];
			if(!node.hasChildren()) return false;
			for(uint8 child_index = 0; child_index < 8; ++child_index)
			{
				const PlanetTerrainNodeId child_id = node.children[child_index];
				if(child_id == invalid_node || !isNodeRelevant(context, context.tree.nodes[child_id])) continue;
				if(!hasCurrentMesh(context, child_id)) return false;
			}
			// A parent can intersect the view while all its children are culled. In that
			// case the parent is only a conservative visibility bound.
			return true;
		};

		const auto activate = [&](auto&& self, PlanetTerrainNodeId node_id) -> void
		{
			PlanetTerrainNode& node = context.tree.nodes[node_id];
			if(!node.hasChildren()
				|| node.half_extent <= getTargetHalfExtent(context, node.center))
			{
				if(hasCurrentMesh(context, node_id)) node.render_active = true;
				return;
			}

			if(children_ready(node_id))
			{
				for(uint8 child_index = 0; child_index < 8; ++child_index)
				{
					const PlanetTerrainNodeId child_id = node.children[child_index];
					if(child_id != invalid_node && isNodeRelevant(context, context.tree.nodes[child_id]))
						self(self, child_id);
				}
			}
			else if(hasCurrentMesh(context, node_id)
				&& node.half_extent <= getTargetHalfExtent(context, node.center)
					* fallback_size_factor)
			{
				node.render_active = true;
			}
			else
			{
				for(uint8 child_index = 0; child_index < 8; ++child_index)
				{
					const PlanetTerrainNodeId child_id = node.children[child_index];
					if(child_id != invalid_node && isNodeRelevant(context, context.tree.nodes[child_id]))
						self(self, child_id);
				}
			}
		};
		activate(activate, context.tree.root_id);
	}
}
