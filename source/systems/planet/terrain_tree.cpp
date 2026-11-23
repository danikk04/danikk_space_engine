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
		nodes[root_id].generation_id = next_node_generation_id++;
		if(next_node_generation_id == 0) next_node_generation_id++;
		return root_id;
	}

	bool PlanetTerrainTree::ensureChildren(PlanetTerrainNodeId parent_id)
	{
		if(parent_id >= nodes.size()) return false;
		if(nodes[parent_id].hasChildren()) return true;
		if(nodes.size() + 8 > maximum_node_count || !splitNode(parent_id)) return false;
		for(uint8 child_index = 0; child_index < 8; ++child_index)
		{
			const PlanetTerrainNodeId child_id = nodes[parent_id].children[child_index];
			nodes[child_id].generation_id = next_node_generation_id++;
			if(next_node_generation_id == 0) next_node_generation_id++;
		}
		return true;
	}

	void PlanetSystem::destroyPlanetTerrainTree(PlanetData& planet,
		MeshDeletionReason reason)
	{
		DSE_WATCH();
		for(PlanetTerrainNode& node : planet.terrain_tree.nodes)
		{
			if(node.mesh.mesh == nullptr) continue;
			destroyMeshRenderData(node.mesh);
			world.getTelemetrySystem().mesh_generation_telemetry.recordMeshDeletion(reason);
		}
		planet.terrain_tree.clear();
	}

	void PlanetSystem::updatePlanetTerrainTree(PlanetData& planet,
		const glm::dvec3& camera_position, double height)
	{
		DSE_WATCH();
		const PlanetSettings& planet_settings = getPlanetSettings(planet.settings_id);
		PlanetTerrainTree& tree = planet.terrain_tree;
		glm::dvec3 render_window_minimum(0.0);
		glm::dvec3 render_window_maximum(0.0);
		double detail_scale = 0.0;
		size_t target_node_count = 0;
		size_t detail_scale_adjustments = 0;
		DynamicArray<PlanetTerrainNodeId> selected_node_ids;

		synchronizeTerrainTree(planet);
		PlanetTerrainTree::calculateDetailScale(camera_position, height,
			planet_settings, render_window_minimum, render_window_maximum, detail_scale,
			target_node_count, detail_scale_adjustments);
		planet.streaming_camera_position = camera_position;
		planet.streaming_height = height;
		planet.render_window_minimum = render_window_minimum;
		planet.render_window_maximum = render_window_maximum;
		planet.terrain_streaming_active = true;
		tree.refine(camera_position, height, planet_settings,
			detail_scale, render_window_minimum, render_window_maximum);
		tree.updateRenderActivity(camera_position, height, planet_settings, detail_scale,
			render_window_minimum, render_window_maximum, selected_node_ids);
		tree.activateMeshes(selected_node_ids);
		recordDiagnostics(planet, target_node_count, detail_scale_adjustments, detail_scale);
		evictCachedMeshes(tree, camera_position);
	}
}
