#include <cmath>
#include <limits>

#include <danikk_space_engine/systems/telemetry/telemetry.h>
#include <danikk_space_engine/systems/planet/system.h>
#include <danikk_space_engine/world.h>
#include <danikk_space_engine/systems/camera/camera.h>

namespace danikk_space_engine
{
	bool PlanetSystem::findNextMeshGenerationJob(MeshGenerationJob& job,
		double& best_distance_squared)
	{
		DSE_WATCH();
		if(!world.getCameraSystem().isActive()) return false;
		const glm::dvec3 camera_world_position = world.getCameraSystem().getWorldPosition();
		constexpr int window_radius = 4;
		const double root_half_extent = PlanetTerrainTree::initial_root_half_extent;
		const double root_side = root_half_extent * 2.0;
		PlanetData* selected_planet = nullptr;
		PlanetTerrainNodeId selected_node_id = PlanetTerrainTree::invalid_node;
		best_distance_squared = std::numeric_limits<double>::infinity();

		for(PlanetData& planet : planets)
		{
			if(!planet.terrain_streaming_active) continue;
			const PlanetSettings& planet_settings = getPlanetSettings(planet.settings_id);
			const glm::dvec3 camera_position = glm::conjugate(planet.rotation)
				* (camera_world_position - planet.position);
			glm::dvec3 anchor = camera_position;
			if(planet.streaming_height >= 0.0)
			{
				const double camera_radius = glm::length(camera_position);
				if(camera_radius > 1e-9)
					anchor *= (camera_radius - planet.streaming_height) / camera_radius;
			}

			const double target_half_extent = PlanetTerrainTree::getTargetHalfExtent(
				camera_position, planet.streaming_detail_scale, anchor);
			uint8 start_depth = 0;
			double level_half_extent = root_half_extent;
			while(start_depth < PlanetTerrainTree::maximum_depth
				&& level_half_extent > target_half_extent)
			{
				level_half_extent *= 0.5;
				start_depth++;
			}

			PlanetTerrainTree& tree = planet.terrain_tree;
			if(tree.root_id == PlanetTerrainTree::invalid_node && tree.createRoot()
				== PlanetTerrainTree::invalid_node) continue;
			for(int depth = static_cast<int>(start_depth); depth >= 0; --depth)
			{
				const double node_width = root_side / static_cast<double>(uint64(1) << depth);
				const int64 cell_count = int64(1) << depth;
				int64 anchor_cell[3];
				for(uint8 axis = 0; axis < 3; ++axis)
				{
					const double coordinate = (anchor[axis] + root_half_extent) / node_width;
					anchor_cell[axis] = static_cast<int64>(std::floor(coordinate));
					anchor_cell[axis] = glm::clamp(anchor_cell[axis], int64(0), cell_count - 1);
				}

				bool level_has_uncovered_cell = false;
				PlanetTerrainNodeId nearest_node_id = PlanetTerrainTree::invalid_node;
				double nearest_node_distance_squared = std::numeric_limits<double>::infinity();
				for(int z = -window_radius; z <= window_radius; ++z)
				for(int y = -window_radius; y <= window_radius; ++y)
				for(int x = -window_radius; x <= window_radius; ++x)
				{
					const int64 cell_x = anchor_cell[0] + x;
					const int64 cell_y = anchor_cell[1] + y;
					const int64 cell_z = anchor_cell[2] + z;
					if(cell_x < 0 || cell_y < 0 || cell_z < 0
						|| cell_x >= cell_count || cell_y >= cell_count || cell_z >= cell_count) continue;
					const glm::dvec3 cell_center(
						-root_half_extent + (static_cast<double>(cell_x) + 0.5) * node_width,
						-root_half_extent + (static_cast<double>(cell_y) + 0.5) * node_width,
						-root_half_extent + (static_cast<double>(cell_z) + 0.5) * node_width);
					PlanetTerrainNode candidate;
					candidate.center = cell_center;
					candidate.half_extent = node_width * 0.5;
					if(!candidate.isRelevantTo(planet.render_window_minimum,
						planet.render_window_maximum, camera_position, planet.streaming_height,
						planet_settings)) continue;

					PlanetTerrainNodeId node_id = tree.root_id;
					bool node_available = true;
					for(int node_depth = 0; node_depth < depth; ++node_depth)
					{
						const PlanetTerrainNode& parent = tree.nodes[node_id];
						const uint8 child_index = static_cast<uint8>(
							(cell_center.x >= parent.center.x ? 1 : 0)
							| (cell_center.y >= parent.center.y ? 2 : 0)
							| (cell_center.z >= parent.center.z ? 4 : 0));
						if(!tree.ensureChildren(node_id))
						{
							node_available = false;
							break;
						}
						node_id = tree.nodes[node_id].children[child_index];
						if(node_id == PlanetTerrainTree::invalid_node)
						{
							node_available = false;
							break;
						}
					}
					if(!node_available || node_id >= tree.nodes.size())
					{
						level_has_uncovered_cell = true;
						continue;
					}
					const PlanetTerrainNode& node = tree.nodes[node_id];

					const bool surface_only = node.intersectsSurfaceBand(planet_settings.radius,
						25000.0, 15000.0);
					const bool mesh_current = node.hasCurrentMesh(planet.terrain_revision,
						settings.planet_mesh_generator, surface_only);
					const bool job_current = node.generation_pending
						&& node.pending_terrain_revision == planet.terrain_revision
						&& node.pending_node_generation_id == node.generation_id
						&& node.pending_mesh_generator == settings.planet_mesh_generator
						&& node.pending_surface_only == surface_only;
					if(mesh_current || job_current) continue;

					level_has_uncovered_cell = true;
					const glm::dvec3 node_world_position = planet.position + planet.rotation * node.center;
					const double center_distance = glm::length(node_world_position - camera_world_position);
					const double distance_to_node = glm::max(0.0,
						center_distance - node.half_extent * 1.7320508075688772);
					const double distance_squared = distance_to_node * distance_to_node;
					if(distance_squared < nearest_node_distance_squared)
					{
						nearest_node_distance_squared = distance_squared;
						nearest_node_id = node_id;
					}
				}

				if(!level_has_uncovered_cell) continue;
				if(nearest_node_id != PlanetTerrainTree::invalid_node
					&& nearest_node_distance_squared < best_distance_squared)
				{
					selected_planet = &planet;
					selected_node_id = nearest_node_id;
					best_distance_squared = nearest_node_distance_squared;
				}
				break;
			}
		}

		if(selected_planet == nullptr || selected_node_id == PlanetTerrainTree::invalid_node)
			return false;
		const PlanetTerrainNode& node = selected_planet->terrain_tree.nodes[selected_node_id];
		job = MeshGenerationJob();
		job.source = MeshGenerationSource::planet_terrain_node;
		job.planet_id = selected_planet->id;
		job.planet_generation_id = selected_planet->generation_id;
		job.terrain_revision = selected_planet->terrain_revision;
		job.terrain_node_id = selected_node_id;
		job.terrain_node_generation_id = node.generation_id;
		job.surface_only = node.intersectsSurfaceBand(
			getPlanetSettings(selected_planet->settings_id).radius, 25000.0, 15000.0);
		job.planet_mesh_generator = settings.planet_mesh_generator;
		job.node_width = node.half_extent * 2.0;
		job.voxel_size = glm::max(2.0, job.node_width / 16.0);
		job.node_minimum = node.center - glm::dvec3(node.half_extent);
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
		if(!node.isRelevantTo(planet->render_window_minimum, planet->render_window_maximum,
			planet->streaming_camera_position, planet->streaming_height, planet_settings)) return false;
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
