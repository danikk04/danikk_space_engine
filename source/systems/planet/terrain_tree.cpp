#include <danikk_space_engine/systems/telemetry/telemetry.h>
#include <danikk_space_engine/systems/planet/system.h>
#include <danikk_space_engine/world.h>

namespace danikk_space_engine
{
	bool PlanetTerrainNode::intersectsBounds(const glm::dvec3& minimum,
		const glm::dvec3& maximum) const
	{
		const glm::dvec3 node_minimum = center - glm::dvec3(half_extent);
		const glm::dvec3 node_maximum = center + glm::dvec3(half_extent);
		for(uint8 axis = 0; axis < 3; ++axis)
			if(node_maximum[axis] < minimum[axis] || node_minimum[axis] > maximum[axis]) return false;
		return true;
	}

	bool PlanetTerrainNode::intersectsSphere(const glm::dvec3& sphere_center,
		double sphere_radius) const
	{
		const glm::dvec3 node_minimum = center - glm::dvec3(half_extent);
		const glm::dvec3 node_maximum = center + glm::dvec3(half_extent);
		const glm::dvec3 closest_point = glm::clamp(sphere_center, node_minimum, node_maximum);
		const glm::dvec3 delta = sphere_center - closest_point;
		return glm::dot(delta, delta) <= sphere_radius * sphere_radius;
	}

	bool PlanetTerrainNode::intersectsSurfaceBand(double planet_radius,
		double below_surface, double above_surface) const
	{
		const double radial_extent = half_extent * 1.7320508075688772;
		const double center_radius = glm::length(center);
		return center_radius + radial_extent >= planet_radius - below_surface
			&& glm::max(0.0, center_radius - radial_extent) <= planet_radius + above_surface;
	}

	bool PlanetTerrainNode::hasCurrentMesh(uint64 terrain_revision,
		PlanetMeshGenerator expected_generator, bool expected_surface_only) const
	{
		return mesh_revision == terrain_revision && mesh_generator == expected_generator
			&& mesh_surface_only == expected_surface_only && mesh_generated;
	}

	PlanetTerrainNodeId PlanetTerrainTree::createRoot()
	{
		if(root_id != invalid_node) return root_id;
		const PlanetTerrainNodeId root_id = danikk_framework::Octree<
			PlanetTerrainNode, PlanetTerrainNodeId>::createRoot(
				glm::dvec3(0.0), initial_root_half_extent);
		if(root_id == invalid_node) return invalid_node;
		nodes[root_id].parent_id = invalid_node;
		nodes[root_id].depth = 0;
		nodes[root_id].generation_id = next_node_generation_id++;
		if(next_node_generation_id == 0) next_node_generation_id++;
		return root_id;
	}

	bool PlanetTerrainTree::ensureChildren(PlanetTerrainNodeId parent_id)
	{
		if(parent_id >= nodes.size()) return false;
		if(nodes[parent_id].hasChildren()) return true;
		const uint8 child_depth = static_cast<uint8>(nodes[parent_id].depth + 1);
		if(nodes.size() + 8 > maximum_node_count || !splitNode(parent_id)) return false;
		for(uint8 child_index = 0; child_index < 8; ++child_index)
		{
			const PlanetTerrainNodeId child_id = nodes[parent_id].children[child_index];
			nodes[child_id].parent_id = parent_id;
			nodes[child_id].depth = child_depth;
			nodes[child_id].generation_id = next_node_generation_id++;
			if(next_node_generation_id == 0) next_node_generation_id++;
		}
		return true;
	}

	void PlanetData::destroyTerrainTree(MeshGenerationTelemetry& telemetry,
		MeshDeletionReason reason)
	{
		DSE_WATCH();
		for(PlanetTerrainNode& node : terrain_tree.nodes)
		{
			if(node.mesh.mesh == nullptr) continue;
			destroyMeshRenderData(node.mesh);
			telemetry.recordMeshDeletion(reason);
		}
		terrain_tree.clear();
	}

	void PlanetData::applyTerrainChangesToTree(MeshGenerationTelemetry& telemetry)
	{
		DSE_WATCH();
		PlanetTerrainTree& tree = terrain_tree;
		if(tree.root_id == PlanetTerrainTree::invalid_node) tree.createRoot();

		if(terrain_tree_revision != terrain_revision)
		{
			for(PlanetTerrainNode& node : tree.nodes)
			{
				node.generation_pending = false;
				node.pending_terrain_revision = 0;
				node.pending_node_generation_id = 0;
				bool dirty = pending_terrain_dirty_regions.empty();
				for(const PlanetTerrainDirtyRegion& region : pending_terrain_dirty_regions)
				{
					if(!node.intersectsSphere(region.center, region.radius)) continue;
					dirty = true;
					break;
				}
				if(dirty)
				{
					node.mesh_revision = 0;
					node.mesh_generated = false;
					node.render_active = false;
					node.generation_id = tree.next_node_generation_id++;
					if(tree.next_node_generation_id == 0) tree.next_node_generation_id++;
				}
				else if(node.mesh_generated)
					node.mesh_revision = terrain_revision;
			}
			pending_terrain_dirty_regions.clear();
			terrain_tree_revision = terrain_revision;
		}

		if(tree.nodes.size() + 8192 <= PlanetTerrainTree::maximum_node_count) return;
		for(PlanetTerrainNode& node : tree.nodes)
		{
			if(node.mesh.mesh == nullptr) continue;
			destroyMeshRenderData(node.mesh);
			telemetry.recordMeshDeletion(MeshDeletionReason::tree_node_limit);
		}
		tree.clear();
		tree.createRoot();
	}

	void PlanetData::updateTerrainStreaming(const glm::dvec3& camera_position, double height,
		const PlanetSettings& planet_settings, MeshGenerationTelemetry& telemetry)
	{
		DSE_WATCH();
		double detail_scale = 0.0;

		applyTerrainChangesToTree(telemetry);
		PlanetTerrainTree::calculateStreamingDetailScale(camera_position, height,
			planet_settings, render_window_minimum, render_window_maximum, detail_scale,
			streaming_target_node_count, streaming_detail_scale_adjustments);
		streaming_camera_position = camera_position;
		streaming_height = height;
		streaming_detail_scale = detail_scale;
		terrain_streaming_active = true;
		terrain_tree.refine(camera_position, height, planet_settings,
			detail_scale, render_window_minimum, render_window_maximum);
	}
}
