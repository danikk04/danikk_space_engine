#include <algorithm>
#include <danikk_space_engine/systems/telemetry/telemetry.h>
#include <danikk_space_engine/systems/planet/system.h>

namespace danikk_space_engine
{
	size_t PlanetSystem::collectMeshJobCandidates(PlanetData& planet,
		const glm::dvec3& camera_position, double height, const PlanetSettings& planet_settings,
		double detail_scale, const glm::dvec3& render_window_minimum,
		const glm::dvec3& render_window_maximum,
		DynamicArray<PlanetTerrainNodeId>& selected_node_ids)
	{
		DSE_WATCH();
		PlanetTerrainTree& tree = planet.terrain_tree;
		for(PlanetTerrainNode& node : tree.nodes) node.render_active = false;
		planet.pending_job_candidates.clear();
		for(PlanetTerrainNode& node : tree.nodes) node.generation_pending = false;
		selected_node_ids.clear();
		DynamicArray<PlanetTerrainNodeId> visible_node_ids;
		DynamicArray<PlanetTerrainNodeId> generation_node_ids;

		const auto collect = [&](auto&& self, PlanetTerrainNodeId node_id) -> void
		{
			const PlanetTerrainNode& node = tree.nodes[node_id];
			if(node.half_extent > getTargetHalfExtent(camera_position, detail_scale, node.center))
			{
				if(!node.hasChildren()) return;
				for(uint8 child_index = 0; child_index < 8; ++child_index)
				{
					const PlanetTerrainNodeId child_id = node.children[child_index];
					if(child_id != PlanetTerrainTree::invalid_node && isNodeRelevant(render_window_minimum,
						render_window_maximum, camera_position, height, planet_settings,
						tree.nodes[child_id]))
						self(self, child_id);
				}
				return;
			}
			visible_node_ids.push(node_id);
		};
		if(tree.root_id >= tree.nodes.size()) return 0;
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
		}
		for(PlanetTerrainNodeId node_id : selected_node_ids)
			tree.nodes[node_id].render_active = true;

		// Keep coarse ancestor meshes warm alongside the active fine frontier. They
		// provide a ready level when the camera moves away from the surface.
		const auto collect_generation_nodes = [&](auto&& self, PlanetTerrainNodeId node_id) -> void
		{
			if(node_id >= tree.nodes.size()) return;
			const PlanetTerrainNode& node = tree.nodes[node_id];
			if(!isNodeRelevant(render_window_minimum, render_window_maximum, camera_position,
				height, planet_settings, node)) return;
			generation_node_ids.push(node_id);
			for(PlanetTerrainNodeId child_id : node.children)
				if(child_id != PlanetTerrainTree::invalid_node) self(self, child_id);
		};
		collect_generation_nodes(collect_generation_nodes, tree.root_id);
		std::sort(generation_node_ids.begin(), generation_node_ids.end(),
			[&tree, &camera_position](PlanetTerrainNodeId left_id,
				PlanetTerrainNodeId right_id)
			{
				const PlanetTerrainNode& left_node = tree.nodes[left_id];
				const PlanetTerrainNode& right_node = tree.nodes[right_id];
				if(left_node.half_extent != right_node.half_extent)
					return left_node.half_extent > right_node.half_extent;
				if(tree.nodes[left_id].render_active != tree.nodes[right_id].render_active)
					return tree.nodes[left_id].render_active;
				const glm::dvec3 left_offset = left_node.center - camera_position;
				const glm::dvec3 right_offset = right_node.center - camera_position;
				const double left_distance = glm::dot(left_offset, left_offset);
				const double right_distance = glm::dot(right_offset, right_offset);
				return left_distance == right_distance ? left_id < right_id
					: left_distance < right_distance;
			});

		PlanetTerrainNodeId nearest_nodes_per_level[PlanetTerrainTree::maximum_depth + 1]
			[maximum_visible_meshes_per_level] = {};
		for(size_t index = 0; index <= PlanetTerrainTree::maximum_depth; ++index)
			selected_per_level[index] = 0;
		for(PlanetTerrainNodeId node_id : generation_node_ids)
		{
			const uint8 depth = getNodeDepth(node_id);
			if(selected_per_level[depth] >= maximum_visible_meshes_per_level) continue;
			nearest_nodes_per_level[depth][selected_per_level[depth]++] = node_id;
		}

		for(size_t rank = 0; planet.pending_job_candidates.size() < maximum_job_candidates_per_frame;
			++rank)
		{
			bool found_node = false;
			for(uint8 depth = 0; depth <= PlanetTerrainTree::maximum_depth
				&& planet.pending_job_candidates.size() < maximum_job_candidates_per_frame; ++depth)
			{
				if(rank >= selected_per_level[depth]) continue;
				found_node = true;
				const PlanetTerrainNodeId node_id = nearest_nodes_per_level[depth][rank];
				PlanetTerrainNode& node = tree.nodes[node_id];
				const bool surface_only = node.intersectsSurfaceBand(planet_settings.radius,
					25000.0, 15000.0);
				if(node.hasCurrentMesh(planet.terrain_revision,
					::danikk_space_engine::settings.planet_mesh_generator, surface_only)) continue;
				MeshGenerationJob candidate;
				candidate.source = MeshGenerationSource::planet_terrain_node;
				candidate.planet_id = planet.id;
				candidate.planet_generation_id = planet.generation_id;
				candidate.terrain_revision = planet.terrain_revision;
				candidate.terrain_node_id = node_id;
				candidate.terrain_node_generation_id = node.generation_id;
				candidate.surface_only = surface_only;
				candidate.planet_mesh_generator = ::danikk_space_engine::settings.planet_mesh_generator;
				candidate.node_width = node.half_extent * 2.0;
				candidate.voxel_size = glm::max(2.0, candidate.node_width / 16.0);
				candidate.node_minimum = node.center - glm::dvec3(node.half_extent);
				planet.pending_job_candidates.pushMove(candidate);
				node.generation_pending = true;
			}
			if(!found_node) break;
		}
		return selected_node_ids.size();
	}
}
