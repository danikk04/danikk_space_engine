#include <algorithm>
#include <danikk_space_engine/profiler.h>
#include <danikk_space_engine/systems/planet/system.h>
#include <danikk_space_engine/systems/chunk/manager.h>
#include <danikk_space_engine/world.h>
#include <danikk_engine/danikk_engine.h>
#include <danikk_framework/log.h>

namespace danikk_space_engine
{
	namespace
	{
		constexpr PlanetSurfaceNodeId invalid_surface_node = static_cast<PlanetSurfaceNodeId>(-1);
		constexpr size_t maximum_visible_surface_nodes = 2048;
		constexpr size_t maximum_requests_per_planet_frame = 32;
		constexpr size_t maximum_surface_tree_nodes = 131072;
		constexpr double maximum_fallback_size_factor = 4.0;
		constexpr bool log_surface_tree_diagnostics = true;

		bool isNodeRelevant(const PlanetSurfaceNode& node, const glm::dvec3& camera_position,
			double planet_radius, double height)
		{
			const double node_radius = node.half_extent * 1.7320508075688772;
			const double center_distance = glm::length(node.center);
			const double minimum_radius = glm::max(0.0, center_distance - node_radius);
			const double maximum_radius = center_distance + node_radius;
			const bool surface_intersection = height >= 0.0
				&& maximum_radius >= planet_radius - 25000.0
				&& minimum_radius <= planet_radius + 15000.0;
			const glm::dvec3 to_node = node.center - camera_position;
			const double distance = glm::length(to_node);
			const bool local_cave_intersection = height < 0.0
				&& distance <= node_radius + 384.0;
			if(!surface_intersection && !local_cave_intersection) return false;

			if(surface_intersection && height > 0.0 && center_distance > 1.0)
			{
				const double camera_distance = glm::length(camera_position);
				if(camera_distance > planet_radius + 1.0)
				{
					const double horizon_cosine = planet_radius / camera_distance;
					const glm::dvec3 camera_direction = camera_position / camera_distance;
					const glm::dvec3 node_direction = node.center / center_distance;
					const double angular_radius = std::min(1.0, node_radius / center_distance);
					if(glm::dot(camera_direction, node_direction) + angular_radius < horizon_cosine)
						return false;
				}
			}
			return true;
		}
	}

	void PlanetSystem::destroyPlanetSurfaceTree(PlanetData& planet)
	{
		DSE_WATCH();
		for(PlanetSurfaceNode& node : planet.surface_tree.nodes)
			if(node.mesh.mesh != nullptr) destroyMeshRenderData(node.mesh);
		planet.surface_tree.nodes.clear();
		planet.surface_tree.root_id = invalid_surface_node;
	}

	void PlanetSystem::updatePlanetSurfaceTree(PlanetData& planet,
		const glm::dvec3& camera_position, double height)
	{
		DSE_WATCH();
		PlanetSurfaceTree& tree = planet.surface_tree;
		if(tree.root_id == invalid_surface_node)
		{
			PlanetSurfaceNode root;
			root.center = glm::dvec3(0.0);
			root.half_extent = PlanetSurfaceTree::initial_root_half_extent;
			root.generation_id = tree.next_node_generation_id++;
			if(tree.next_node_generation_id == 0) tree.next_node_generation_id++;
			tree.nodes.pushMove(root);
			tree.root_id = 0;
		}

		if(planet.surface_tree_revision != planet.terrain_revision)
		{
			for(PlanetSurfaceNode& node : tree.nodes)
			{
				node.generation_pending = false;
				node.generation_id = tree.next_node_generation_id++;
				if(tree.next_node_generation_id == 0) tree.next_node_generation_id++;
				node.render_active = false;
			}
			planet.surface_tree_revision = planet.terrain_revision;
		}

		const PlanetSettings& planet_settings = getPlanetSettings(planet.settings_id);
		double detail_scale = height < 0.0 ? 0.01 : 0.015;
		const auto target_half_extent_for = [&camera_position, height, &detail_scale](
			const glm::dvec3& center, double half_extent)
		{
			const double distance = glm::length(camera_position - center);
			const double minimum = height < 0.0 ? 16.0 : 32.0;
			return std::clamp(minimum + detail_scale * distance, minimum, 262144.0);
		};
		const auto count_visible_target_nodes = [&](auto&& self, const glm::dvec3& center,
			double half_extent, uint8 depth, size_t limit) -> size_t
		{
			PlanetSurfaceNode candidate;
			candidate.center = center;
			candidate.half_extent = half_extent;
			if(!isNodeRelevant(candidate, camera_position, planet_settings.radius, height)) return 0;
			if(half_extent <= target_half_extent_for(center, half_extent) || depth >= 20) return 1;
			const double child_half_extent = half_extent * 0.5;
			size_t count = 0;
			for(uint8 child_index = 0; child_index < 8; ++child_index)
			{
				const glm::dvec3 child_center = center + glm::dvec3(
					(child_index & 1) ? child_half_extent : -child_half_extent,
					(child_index & 2) ? child_half_extent : -child_half_extent,
					(child_index & 4) ? child_half_extent : -child_half_extent);
				count += self(self, child_center, child_half_extent,
					static_cast<uint8>(depth + 1), limit - count);
				if(count > limit) return count;
			}
			return count;
		};
		size_t target_node_count = count_visible_target_nodes(count_visible_target_nodes,
			glm::dvec3(0.0), PlanetSurfaceTree::initial_root_half_extent, 0,
			maximum_visible_surface_nodes);
		size_t detail_scale_adjustments = 0;
		while(target_node_count > maximum_visible_surface_nodes && detail_scale_adjustments < 32)
		{
			detail_scale *= 1.5;
			detail_scale_adjustments++;
			target_node_count = count_visible_target_nodes(count_visible_target_nodes,
				glm::dvec3(0.0), PlanetSurfaceTree::initial_root_half_extent, 0,
				maximum_visible_surface_nodes);
		}

		if(tree.nodes.size() + 8192 > maximum_surface_tree_nodes)
		{
			for(PlanetSurfaceNode& node : tree.nodes)
				if(node.mesh.mesh != nullptr) destroyMeshRenderData(node.mesh);
			tree.nodes.clear();
			tree.root_id = invalid_surface_node;
			PlanetSurfaceNode root;
			root.center = glm::dvec3(0.0);
			root.half_extent = PlanetSurfaceTree::initial_root_half_extent;
			root.generation_id = tree.next_node_generation_id++;
			if(tree.next_node_generation_id == 0) tree.next_node_generation_id++;
			tree.nodes.pushMove(root);
			tree.root_id = 0;
		}

		const auto ensure_children = [&tree](PlanetSurfaceNodeId parent_id)
		{
			if(tree.nodes[parent_id].hasChildren()) return true;
			if(tree.nodes.size() + 8 > maximum_surface_tree_nodes) return false;
			const PlanetSurfaceNode& parent = tree.nodes[parent_id];
			const glm::dvec3 parent_center = parent.center;
			const double child_half_extent = parent.half_extent * 0.5;
			for(uint8 child_index = 0; child_index < 8; ++child_index)
			{
				PlanetSurfaceNode child;
				child.center = parent_center + glm::dvec3(
					(child_index & 1) ? child_half_extent : -child_half_extent,
					(child_index & 2) ? child_half_extent : -child_half_extent,
					(child_index & 4) ? child_half_extent : -child_half_extent);
				child.half_extent = child_half_extent;
				child.generation_id = tree.next_node_generation_id++;
				if(tree.next_node_generation_id == 0) tree.next_node_generation_id++;
				const PlanetSurfaceNodeId child_id = static_cast<PlanetSurfaceNodeId>(tree.nodes.size());
				tree.nodes.pushMove(child);
				tree.nodes[parent_id].children[child_index] = child_id;
			}
			return true;
		};

		const auto visit = [&](auto&& self, PlanetSurfaceNodeId node_id, uint8 depth) -> void
		{
			if(node_id >= tree.nodes.size() || depth >= 20) return;
			PlanetSurfaceNode candidate;
			candidate.center = tree.nodes[node_id].center;
			candidate.half_extent = tree.nodes[node_id].half_extent;
			if(!isNodeRelevant(candidate, camera_position, planet_settings.radius, height)) return;
			if(candidate.half_extent <= target_half_extent_for(candidate.center, candidate.half_extent))
			{
				return;
			}
			if(!ensure_children(node_id))
			{
				return;
			}
			for(uint8 child_index = 0; child_index < 8; ++child_index)
				self(self, tree.nodes[node_id].children[child_index], static_cast<uint8>(depth + 1));
		};
		visit(visit, tree.root_id, 0);

		for(PlanetSurfaceNode& node : tree.nodes)
			node.render_active = false;

		planet.pending_requests.clear();
		for(PlanetSurfaceNode& node : tree.nodes) node.generation_pending = false;
		const auto is_surface_node = [&planet_settings](const PlanetSurfaceNode& node)
		{
			const double radial_extent = node.half_extent * 1.7320508075688772;
			const double center_radius = glm::length(node.center);
			return center_radius + radial_extent >= planet_settings.radius - 25000.0
				&& glm::max(0.0, center_radius - radial_extent) <= planet_settings.radius + 15000.0;
		};
		const auto append_request = [&](PlanetSurfaceNodeId node_id)
		{
			PlanetSurfaceNode& node = tree.nodes[node_id];
			const bool surface_only = is_surface_node(node);
			if(node.mesh_revision == planet.terrain_revision
				&& node.mesh_generator == settings.planet_mesh_generator
				&& node.mesh_surface_only == surface_only
				&& node.mesh.mesh != nullptr) return;
			if(planet.pending_requests.size() >= maximum_requests_per_planet_frame) return;
			ChunkMeshGenerationRequest request;
			request.source = ChunkMeshSource::planet_surface_node;
			request.world_body_id = planet.world_body_id;
			request.planet_generation_id = planet.generation_id;
			request.terrain_revision = planet.terrain_revision;
			request.surface_node_id = node_id;
			request.surface_node_generation_id = node.generation_id;
			request.surface_only = surface_only;
			request.planet_mesh_generator = settings.planet_mesh_generator;
			request.chunk_width = node.half_extent * 2.0;
			request.voxel_size = glm::max(2.0, request.chunk_width / 16.0);
			request.chunk_position = node.center - glm::dvec3(node.half_extent);
			planet.pending_requests.pushMove(request);
			node.generation_pending = true;
		};

		const auto has_current_mesh = [&tree, this, &planet, &is_surface_node](PlanetSurfaceNodeId node_id)
		{
			const PlanetSurfaceNode& node = tree.nodes[node_id];
			return node.mesh_revision == planet.terrain_revision
				&& node.mesh_generator == settings.planet_mesh_generator
				&& node.mesh_surface_only == is_surface_node(node)
				&& node.mesh.mesh != nullptr;
		};
		DynamicArray<PlanetSurfaceNodeId> request_nodes;
		const auto collect_requests = [&](auto&& self, PlanetSurfaceNodeId node_id) -> void
		{
			const PlanetSurfaceNode& node = tree.nodes[node_id];
			if(!node.hasChildren() || node.half_extent <= target_half_extent_for(node.center, node.half_extent)
				|| !has_current_mesh(node_id))
			{
				if(!has_current_mesh(node_id)) request_nodes.push(node_id);
				return;
			}
			for(uint8 i = 0; i < 8; ++i)
			{
				const PlanetSurfaceNodeId child_id = node.children[i];
				if(child_id != invalid_surface_node && isNodeRelevant(tree.nodes[child_id],
					camera_position, planet_settings.radius, height)) self(self, child_id);
			}
		};
		collect_requests(collect_requests, tree.root_id);
		std::sort(request_nodes.begin(), request_nodes.end(), [&tree, &camera_position](
			PlanetSurfaceNodeId a_id, PlanetSurfaceNodeId b_id)
		{
			const glm::dvec3 a_offset = tree.nodes[a_id].center - camera_position;
			const glm::dvec3 b_offset = tree.nodes[b_id].center - camera_position;
			return glm::dot(a_offset, a_offset) < glm::dot(b_offset, b_offset);
		});
		for(PlanetSurfaceNodeId node_id : request_nodes) append_request(node_id);
		const auto children_ready = [&](PlanetSurfaceNodeId node_id) -> bool
		{
			const PlanetSurfaceNode& node = tree.nodes[node_id];
			if(!node.hasChildren()) return false;
			for(uint8 i = 0; i < 8; ++i)
			{
				const PlanetSurfaceNodeId child_id = node.children[i];
				if(child_id == invalid_surface_node || !isNodeRelevant(tree.nodes[child_id],
					camera_position, planet_settings.radius, height)) continue;
				if(!has_current_mesh(child_id)) return false;
			}
			// A parent can intersect the view while all its children are culled. In that
			// case the parent is only a conservative visibility bound; there is nothing
			// visible that needs to block refinement.
			return true;
		};
		const auto activate = [&](auto&& self, PlanetSurfaceNodeId node_id) -> void
		{
			PlanetSurfaceNode& node = tree.nodes[node_id];
			if(!node.hasChildren() || node.half_extent <= target_half_extent_for(node.center, node.half_extent))
			{
				if(has_current_mesh(node_id))
					node.render_active = true;
				return;
			}
			if(children_ready(node_id))
			{
				for(uint8 i = 0; i < 8; ++i)
				{
					const PlanetSurfaceNodeId child_id = node.children[i];
					if(child_id != invalid_surface_node && isNodeRelevant(tree.nodes[child_id],
						camera_position, planet_settings.radius, height))
						self(self, child_id);
				}
			}
			else if(has_current_mesh(node_id)
				&& node.half_extent <= target_half_extent_for(node.center, node.half_extent)
					* maximum_fallback_size_factor)
			{
				node.render_active = true;
			}
			else
			{
				for(uint8 i = 0; i < 8; ++i)
				{
					const PlanetSurfaceNodeId child_id = node.children[i];
					if(child_id != invalid_surface_node && isNodeRelevant(tree.nodes[child_id],
						camera_position, planet_settings.radius, height))
						self(self, child_id);
				}
			}
		};
		activate(activate, tree.root_id);
		if constexpr(log_surface_tree_diagnostics)
		{
			static uint32 report_timestamp_ms = 0;
			const uint32 now = danikk_engine::getMsTimestamp();
			if(report_timestamp_ms == 0) report_timestamp_ms = now;
			if(now - report_timestamp_ms >= 1000)
			{
				size_t active_node_count = 0;
				double finest_active_half_extent = 0.0;
				for(const PlanetSurfaceNode& node : tree.nodes)
				{
					if(!node.render_active) continue;
					active_node_count++;
					if(finest_active_half_extent == 0.0
						|| node.half_extent < finest_active_half_extent)
						finest_active_half_extent = node.half_extent;
				}
				size_t fine_request_count = 0;
				double finest_request_width = 0.0;
				for(const ChunkMeshGenerationRequest& request : planet.pending_requests)
				{
					if(request.chunk_width <= 128.0) fine_request_count++;
					if(finest_request_width == 0.0 || request.chunk_width < finest_request_width)
						finest_request_width = request.chunk_width;
				}
				danikk_framework::formatLogInfo(
					"Planet surface tree: body=% height=% detail_scale=% detail_adjustments=% target_nodes=% nodes=% request_candidates=% queued=% fine_queued=% finest_request_width=% active=% finest_active_half_extent=%",
					planet.world_body_id, height, detail_scale, detail_scale_adjustments,
					target_node_count, tree.nodes.size(), request_nodes.size(),
					planet.pending_requests.size(), fine_request_count, finest_request_width,
					active_node_count, finest_active_half_extent);
				report_timestamp_ms = now;
			}
		}

		constexpr size_t maximum_cached_surface_meshes = 1024;
		size_t cached_mesh_count = 0;
		for(const PlanetSurfaceNode& node : tree.nodes)
			if(node.mesh.mesh != nullptr) cached_mesh_count++;
		while(cached_mesh_count > maximum_cached_surface_meshes)
		{
			PlanetSurfaceNodeId eviction_id = invalid_surface_node;
			double farthest_distance_squared = -1.0;
			for(PlanetSurfaceNodeId node_id = 0; node_id < tree.nodes.size(); ++node_id)
			{
				const PlanetSurfaceNode& node = tree.nodes[node_id];
				if(node.render_active || node.generation_pending
					|| node.mesh.mesh == nullptr) continue;
				const glm::dvec3 offset = node.center - camera_position;
				const double distance_squared = glm::dot(offset, offset);
				if(distance_squared <= farthest_distance_squared) continue;
				farthest_distance_squared = distance_squared;
				eviction_id = node_id;
			}
			if(eviction_id == invalid_surface_node) break;
			PlanetSurfaceNode& node = tree.nodes[eviction_id];
			destroyMeshRenderData(node.mesh);
			node.mesh_revision = 0;
			cached_mesh_count--;
		}
		queuePlanetRequests(planet);
	}
}
