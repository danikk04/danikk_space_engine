#include <danikk_space_engine/systems/telemetry/telemetry.h>
#include <danikk_space_engine/systems/planet/system.h>

namespace danikk_space_engine
{
	uint8 PlanetData::getCurrentTerrainDepth() const
	{
		glm::dvec3 anchor = streaming_camera_position;
		if(streaming_height >= 0.0)
		{
			const double camera_radius = glm::length(streaming_camera_position);
			if(camera_radius > 1e-9)
				anchor *= (camera_radius - streaming_height) / camera_radius;
		}

		const double target_half_extent = terrain_tree.getTargetHalfExtent(
			streaming_camera_position, streaming_detail_scale, anchor);
		uint8 depth = 0;
		double level_half_extent = terrain_tree.initial_root_half_extent;
		while(depth < terrain_tree.maximum_depth && level_half_extent > target_half_extent)
		{
			level_half_extent *= 0.5;
			depth++;
		}
		return depth;
	}

	void PlanetData::buildTerrainRenderList(const PlanetSettings& settings)
	{
		DSE_WATCH();
		PlanetTerrainTree& tree = terrain_tree;
		terrain_render_items.clear();
		for(PlanetTerrainNode& node : tree.nodes) node.render_active = false;
		if(tree.root_id >= tree.nodes.size()) return;

		const uint8 current_depth = getCurrentTerrainDepth();
		glm::dvec3 anchor = streaming_camera_position;
		if(streaming_height >= 0.0)
		{
			const double camera_radius = glm::length(streaming_camera_position);
			if(camera_radius > 1e-9)
				anchor *= (camera_radius - streaming_height) / camera_radius;
		}

		// Build render coverage from the fixed depth window alone. Mesh readiness
		// only decides whether a selected cell currently has drawable geometry;
		// it never promotes a coarser parent to fill a missing fine cell.
		for(PlanetTerrainNodeId node_id = 0; node_id < tree.nodes.size(); ++node_id)
		{
			PlanetTerrainNode& node = tree.nodes[node_id];
			if(!tree.isInRenderCoverage(node, anchor, current_depth)) continue;
			if(!node.isInTerrainRenderRegion(render_window_minimum, render_window_maximum,
				streaming_camera_position, streaming_height, settings)) continue;

			const bool expected_surface_only = tree.isSurfaceOnly(node, settings.radius);
			if(node.mesh_surface_only != expected_surface_only) node.mesh_generated = false;
			if(node.mesh.mesh == nullptr || node.mesh.material_ranges.size() == 0
				|| node.mesh_surface_only != expected_surface_only) continue;

			node.render_active = true;
			terrain_render_items.push(PlanetTerrainRenderItem{
				node_id, position + rotation * node.mesh_position, rotation});
		}
	}
}
