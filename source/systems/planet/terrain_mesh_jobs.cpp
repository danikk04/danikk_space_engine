#include <limits>

#include <danikk_space_engine/systems/telemetry/telemetry.h>
#include <danikk_space_engine/systems/planet/system.h>
#include <danikk_space_engine/world.h>
#include <danikk_space_engine/systems/camera/camera.h>

namespace danikk_space_engine
{
	bool PlanetSystem::findNextMeshGenerationJob(MeshGenerationJob& job,
		double& best_distance_squared) const
	{
		DSE_WATCH();
		if(!world.getCameraSystem().isActive()) return false;
		const glm::dvec3 camera_world_position = world.getCameraSystem().getWorldPosition();
		constexpr size_t level_count = PlanetTerrainTree::maximum_depth + 1;
		PlanetTerrainNodeId nearest_node_per_level[level_count];
		const PlanetData* nearest_planet_per_level[level_count] = {};
		double nearest_distance_per_level[level_count];
		bool nearest_is_render_active_per_level[level_count] = {};
		size_t pending_per_level[level_count] = {};
		for(size_t level = 0; level < level_count; ++level)
		{
			nearest_node_per_level[level] = PlanetTerrainTree::invalid_node;
			nearest_distance_per_level[level] = std::numeric_limits<double>::infinity();
		}
		for(const PlanetData& planet : planets)
		{
			if(!planet.terrain_streaming_active) continue;
			const PlanetSettings& planet_settings = getPlanetSettings(planet.settings_id);
			const PlanetTerrainTree& tree = planet.terrain_tree;
			const auto visit = [&](auto&& self, PlanetTerrainNodeId node_id) -> void
			{
				if(node_id >= tree.nodes.size()) return;
				const PlanetTerrainNode& node = tree.nodes[node_id];
				if(!isNodeRelevant(planet.render_window_minimum,
					planet.render_window_maximum, planet.streaming_camera_position,
					planet.streaming_height, planet_settings, node)) return;

				uint8 level = 0;
				double level_half_extent = PlanetTerrainTree::initial_root_half_extent;
				while(level < PlanetTerrainTree::maximum_depth
					&& level_half_extent > node.half_extent)
				{
					level_half_extent *= 0.5;
					level++;
				}

				if(node.generation_pending
					&& node.pending_terrain_revision == planet.terrain_revision
					&& node.pending_node_generation_id == node.generation_id
					&& node.pending_mesh_generator == settings.planet_mesh_generator)
					pending_per_level[level]++;

				if(!node.generation_pending)
				{
					const bool surface_only = node.intersectsSurfaceBand(planet_settings.radius,
						25000.0, 15000.0);
					if(!node.hasCurrentMesh(planet.terrain_revision,
						settings.planet_mesh_generator, surface_only))
					{
						const glm::dvec3 node_world_position = planet.position + planet.rotation * node.center;
						const double center_distance = glm::length(node_world_position - camera_world_position);
						const double distance_to_node = glm::max(0.0,
							center_distance - node.half_extent * 1.7320508075688772);
						const double distance_squared = distance_to_node * distance_to_node;
						const bool is_render_active = node.render_active;
						const bool is_better = nearest_node_per_level[level]
							== PlanetTerrainTree::invalid_node
							|| (is_render_active && !nearest_is_render_active_per_level[level])
							|| (is_render_active == nearest_is_render_active_per_level[level]
								&& distance_squared < nearest_distance_per_level[level]);
						if(is_better)
						{
							nearest_node_per_level[level] = node_id;
							nearest_planet_per_level[level] = &planet;
							nearest_distance_per_level[level] = distance_squared;
							nearest_is_render_active_per_level[level] = is_render_active;
						}
					}
				}
				for(PlanetTerrainNodeId child_id : node.children)
					if(child_id != PlanetTerrainTree::invalid_node) self(self, child_id);
			};
			visit(visit, tree.root_id);
		}

		size_t selected_level = level_count;
		size_t fewest_pending_jobs = std::numeric_limits<size_t>::max();
		for(size_t level = 0; level < level_count; ++level)
		{
			if(nearest_node_per_level[level] == PlanetTerrainTree::invalid_node
				|| pending_per_level[level] >= fewest_pending_jobs) continue;
			selected_level = level;
			fewest_pending_jobs = pending_per_level[level];
		}
		if(selected_level == level_count) return false;

		const PlanetData& planet = *nearest_planet_per_level[selected_level];
		const PlanetTerrainNodeId node_id = nearest_node_per_level[selected_level];
		const PlanetTerrainNode& node = planet.terrain_tree.nodes[node_id];
		job = MeshGenerationJob();
		job.source = MeshGenerationSource::planet_terrain_node;
		job.planet_id = planet.id;
		job.planet_generation_id = planet.generation_id;
		job.terrain_revision = planet.terrain_revision;
		job.terrain_node_id = node_id;
		job.terrain_node_generation_id = node.generation_id;
		job.surface_only = node.intersectsSurfaceBand(
			getPlanetSettings(planet.settings_id).radius, 25000.0, 15000.0);
		job.planet_mesh_generator = settings.planet_mesh_generator;
		job.node_width = node.half_extent * 2.0;
		job.voxel_size = glm::max(2.0, job.node_width / 16.0);
		job.node_minimum = node.center - glm::dvec3(node.half_extent);
		best_distance_squared = nearest_distance_per_level[selected_level];
		return true;
	}

	bool PlanetSystem::isMeshGenerationJobWanted(const MeshGenerationJob& job) const
	{
		if(job.source != MeshGenerationSource::planet_terrain_node
			|| !world.getCameraSystem().isActive()) return false;
		const PlanetData* planet = findPlanet(job.planet_id);
		if(planet == nullptr || !planet->terrain_streaming_active
			|| planet->generation_id != job.planet_generation_id
			|| planet->terrain_revision != job.terrain_revision
			|| job.planet_mesh_generator != settings.planet_mesh_generator
			|| job.terrain_node_id >= planet->terrain_tree.nodes.size()) return false;
		const PlanetTerrainNode& node = planet->terrain_tree.nodes[job.terrain_node_id];
		if(node.generation_id != job.terrain_node_generation_id) return false;
		const PlanetSettings& planet_settings = getPlanetSettings(planet->settings_id);
		if(!isNodeRelevant(planet->render_window_minimum, planet->render_window_maximum,
			planet->streaming_camera_position, planet->streaming_height, planet_settings, node)) return false;
		const bool surface_only = node.intersectsSurfaceBand(planet_settings.radius, 25000.0, 15000.0);
		return surface_only == job.surface_only
			&& !node.hasCurrentMesh(planet->terrain_revision, settings.planet_mesh_generator, surface_only);
	}

	void PlanetSystem::setMeshGenerationJobPending(const MeshGenerationJob& job, bool pending)
	{
		if(job.source != MeshGenerationSource::planet_terrain_node) return;
		PlanetData* planet = findPlanet(job.planet_id);
		if(planet == nullptr || planet->generation_id != job.planet_generation_id
			|| job.terrain_node_id >= planet->terrain_tree.nodes.size()) return;
		PlanetTerrainNode& node = planet->terrain_tree.nodes[job.terrain_node_id];
		if(node.generation_id != job.terrain_node_generation_id) return;
		if(pending)
		{
			node.generation_pending = true;
			node.pending_terrain_revision = job.terrain_revision;
			node.pending_node_generation_id = job.terrain_node_generation_id;
			node.pending_mesh_generator = job.planet_mesh_generator;
			node.pending_surface_only = job.surface_only;
			return;
		}
		if(!node.generation_pending
			|| node.pending_terrain_revision != job.terrain_revision
			|| node.pending_node_generation_id != job.terrain_node_generation_id
			|| node.pending_mesh_generator != job.planet_mesh_generator
			|| node.pending_surface_only != job.surface_only) return;
		node.generation_pending = false;
		node.pending_terrain_revision = 0;
		node.pending_node_generation_id = 0;
	}
}
