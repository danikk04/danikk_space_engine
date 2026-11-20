#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <memory>
#include <new>

#include <danikk_space_engine/profiler.h>
#include <danikk_space_engine/systems/planet_lod/planet_lod.h>
#include <danikk_space_engine/systems/planet/planet.h>
#include <danikk_space_engine/world.h>
#include <danikk_space_engine/material.h>
#include <danikk_space_engine/assets/collection.h>

#include <danikk_engine/texture_methods.h>
#include <danikk_framework/log.h>

namespace danikk_space_engine
{
	namespace
	{
		constexpr uint32 lod_resolutions[planet_lod_level_count] = {200, 256, 128, 64, 32, 16};
		constexpr double level_altitude_ratios[planet_lod_level_count - 1] = {
			0.001, 0.01, 0.05, 0.25, 0.75};
		constexpr double lod_activation_height = 100.0;
		constexpr double lod_deactivation_height = 70.0;
		constexpr uint8 local_patch_index = 0;
		constexpr uint32 global_face_patch_count = planet_lod_face_count;

		/** @brief Projects local tile coordinates onto the dominant normal plane. */
		vec2 projectLocalPositionToUv(const vec3& position, const vec3& normal)
		{
			const vec3 absolute_normal = glm::abs(normal);
			if(absolute_normal.x >= absolute_normal.y && absolute_normal.x >= absolute_normal.z)
				return vec2(position.z, position.y);
			if(absolute_normal.y >= absolute_normal.z) return vec2(position.x, position.z);
			return vec2(position.x, position.y);
		}

		/** @brief Creates an immutable worker snapshot from the current planet surface state. */
		std::shared_ptr<const PlanetTerrainSnapshot> createLodSnapshot(const PlanetData& planet)
		{
			std::shared_ptr<PlanetTerrainSnapshot> snapshot = std::make_shared<PlanetTerrainSnapshot>();
			snapshot->world_body_id = planet.world_body_id;
			snapshot->planet_generation_id = planet.generation_id;
			snapshot->terrain_revision = planet.terrain_revision;
			snapshot->planet_radius = planet.radius;
			snapshot->core_radius = planet.core_radius;
			snapshot->noise_seed = planet.noise_seed;
			snapshot->terrain_edits = planet.terrain_edits;
			return snapshot;
		}

		/** @brief Returns whether all tiles required by a level are available for drawing. */
		bool isLevelReady(const PlanetLodLevel& level)
		{
			if(!level.initialized || level.mesh == nullptr
				|| level.patches.size() != (level.camera_local ? 1 : planet_lod_face_count)) return false;
			for(const PlanetLodPatch& patch : level.patches)
				if(!patch.ready) return false;
			return true;
		}

		/** @brief Builds the camera-relative transform for a planet-centered surface mesh. */
		mat4 createPlanetTransform(const World& world, const WorldBody& body,
			const glm::dvec3& local_origin = glm::dvec3(0.0))
		{
			const glm::dvec3 relative_position = body.position + body.rotation * local_origin
				- world.camera.getWorldPosition();
			mat4 rotation(1.0f);
			rotation[0] = vec4(vec3(body.rotation * glm::dvec3(1.0, 0.0, 0.0)), 0.0f);
			rotation[1] = vec4(vec3(body.rotation * glm::dvec3(0.0, 1.0, 0.0)), 0.0f);
			rotation[2] = vec4(vec3(body.rotation * glm::dvec3(0.0, 0.0, 1.0)), 0.0f);
			return glm::translate(mat4(1.0f), vec3(relative_position)) * rotation;
		}

		/** @brief Releases a GPU mesh owned by one planetary LOD level. */
		void destroyMesh(Mesh*& mesh)
		{
			if(mesh != nullptr)
			{
				mesh->free();
				std::destroy_at(mesh);
				free(mesh);
				mesh = nullptr;
			}
		}

		/** @brief Combines generated tile payloads into one material-grouped GPU mesh. */
		void uploadLevelMesh(PlanetLodLevel& level)
		{
			DynamicArray<DefaultVertex> vertices;
			DynamicArray<uint32> triangle_indices;
			DynamicArray<MaterialId> triangle_material_ids;
			size_t total_index_count = 0;
			for(const PlanetLodPatch& patch : level.patches)
				total_index_count += patch.triangle_indices.size();
			vertices.reserve(total_index_count);
			triangle_indices.reserve(total_index_count);
			triangle_material_ids.reserve(total_index_count / 3);

			for(const PlanetLodPatch& patch : level.patches)
			{
				for(size_t triangle = 0; triangle + 2 < patch.triangle_indices.size(); triangle += 3)
				{
					const uint32 source_indices[3] = {
						patch.triangle_indices[triangle],
						patch.triangle_indices[triangle + 1],
						patch.triangle_indices[triangle + 2]};
					if(source_indices[0] >= patch.positions.size()
						|| source_indices[1] >= patch.positions.size()
						|| source_indices[2] >= patch.positions.size())
						danikk_engine::formatFatalError("Generated planet LOD mesh has an invalid vertex index");

					const vec3& a = patch.positions[source_indices[0]];
					const vec3& b = patch.positions[source_indices[1]];
					const vec3& c = patch.positions[source_indices[2]];
					const vec3 cross_product = glm::cross(b - a, c - a);
					const float normal_length = glm::length(cross_product);
					if(normal_length <= 0.000001f) continue;
					const vec3 normal = cross_product / normal_length;
					const vec3 triangle_positions[3] = {a, b, c};
					for(const vec3& position : triangle_positions)
					{
						const glm::dvec3 planet_position = patch.origin + glm::dvec3(position);
						const double planet_position_length = glm::length(planet_position);
						const vec3 radial_normal = planet_position_length > 1e-6
							? vec3(planet_position / planet_position_length) : vec3(0.0f, 0.0f, 1.0f);
						const vec2 uv = level.camera_local
							? projectLocalPositionToUv(position, normal)
							: vec2(
								std::atan2(planet_position.y, planet_position.x)
									* planet_position_length / 32.0,
								std::asin(glm::clamp(static_cast<double>(radial_normal.z), -1.0, 1.0))
									* planet_position_length / 32.0);
						vertices.pushCtor(position, normal, uv);
						triangle_indices.push(static_cast<uint32>(vertices.size() - 1));
					}
					triangle_material_ids.push(patch.triangle_material_ids[triangle / 3]);
				}
			}

			DynamicArray<gl_point_index_t> indices;
			level.material_ranges.clear();
			indices.reserve(triangle_indices.size());
			for(MaterialId material_id = 0; material_id < getMaterialCount(); material_id++)
			{
				const uint32 first_index = static_cast<uint32>(indices.size());
				for(size_t triangle = 0; triangle < triangle_material_ids.size(); triangle++)
				{
					if(triangle_material_ids[triangle] != material_id) continue;
					for(uint8 corner = 0; corner < 3; corner++)
						indices.push(static_cast<gl_point_index_t>(triangle_indices[triangle * 3 + corner]));
				}
				const uint32 index_count = static_cast<uint32>(indices.size()) - first_index;
				if(index_count > 0)
					level.material_ranges.push(PlanetLodMaterialRange{material_id, first_index, index_count});
			}

			destroyMesh(level.mesh);
			void* memory = malloc(sizeof(Mesh));
			if(memory == nullptr) std::abort();
			level.mesh = new (memory) Mesh();
			level.mesh->setData(
				reinterpret_cast<const float*>(vertices.data()),
				vertices.size() * sizeof(DefaultVertex) / sizeof(float), indices.data(), indices.size());
			DefaultVertex::setAttributes();
			level.mesh_origin = level.camera_local && level.patches.size() > 0
				? level.patches[0].origin : glm::dvec3(0.0);
		}
	}

	PlanetLodPlanet& PlanetLodSystem::getOrCreatePlanet(const PlanetData& source)
	{
		for(PlanetLodPlanet& planet : planets)
		{
			if(planet.world_body_id == source.world_body_id
				&& planet.planet_generation_id == source.generation_id) return planet;
		}

		for(size_t index = 0; index < planets.size();)
		{
			if(planets[index].world_body_id != source.world_body_id)
			{
				index++;
				continue;
			}
			destroyLevelMeshes(planets[index]);
			for(size_t shift = index + 1; shift < planets.size(); shift++)
				planets[shift - 1] = static_cast<PlanetLodPlanet&&>(planets[shift]);
			planets.resize(planets.size() - 1);
		}

		PlanetLodPlanet planet;
		planet.world_body_id = source.world_body_id;
		planet.planet_generation_id = source.generation_id;
		planet.terrain_revision = source.terrain_revision;
		planet.levels.resize(planet_lod_level_count);
		for(uint8 level_index = 0; level_index < planet_lod_level_count; level_index++)
		{
			PlanetLodLevel& level = planet.levels[level_index];
			level.resolution = lod_resolutions[level_index];
			level.camera_local = level_index == local_patch_index;
			level.patches.resize(level.camera_local ? 1 : global_face_patch_count);
		}
		planets.pushMove(planet);
		return planets[planets.size() - 1];
	}

	void PlanetLodSystem::destroyLevelMeshes(PlanetLodPlanet& planet)
	{
		for(PlanetLodLevel& level : planet.levels)
		{
			destroyMesh(level.mesh);
			level.material_ranges.clear();
			level.initialized = false;
			level.mesh_origin = glm::dvec3(0.0);
			for(PlanetLodPatch& patch : level.patches)
			{
				patch.positions = DynamicArray<vec3>();
				patch.triangle_indices = DynamicArray<uint32>();
				patch.triangle_material_ids = DynamicArray<MaterialId>();
				patch.origin = glm::dvec3(0.0);
				patch.grid_coordinate = glm::ivec3(0);
				patch.ready = false;
			}
		}
		planet.has_active_level = false;
		planet.terrain_snapshot.reset();
	}

	/** @copydoc PlanetLodSystem::update */
	void PlanetLodSystem::update()
	{
		DSE_WATCH();
		for(size_t index = 0; index < planets.size();)
		{
			if(world.planets.isPlanet(planets[index].world_body_id)
				&& world.world_body_data.isWorldBodyActive(planets[index].world_body_id))
			{
				index++;
				continue;
			}
			DynamicArray<ChunkMeshGenerationRequest> no_requests;
			world.chunk_mesh_generation.replacePlanetLodRequests(
				planets[index].world_body_id, planets[index].planet_generation_id, no_requests);
			destroyLevelMeshes(planets[index]);
			for(size_t shift = index + 1; shift < planets.size(); shift++)
				planets[shift - 1] = static_cast<PlanetLodPlanet&&>(planets[shift]);
			planets.resize(planets.size() - 1);
		}

		for(const PlanetData& source : world.planets.getPlanets())
		{
			if(!world.world_body_data.isWorldBodyActive(source.world_body_id)) continue;
			PlanetLodPlanet& planet = getOrCreatePlanet(source);
			if(planet.terrain_revision != source.terrain_revision)
			{
				destroyLevelMeshes(planet);
				planet.terrain_revision = source.terrain_revision;
			}
			if(planet.terrain_snapshot == nullptr)
				planet.terrain_snapshot = createLodSnapshot(source);

			DynamicArray<ChunkMeshGenerationRequest> requests;
			planet.show_local_patch = false;
			if(!world.camera.isActive()) planet.enabled = false;
			else
			{
				const WorldBody& body = world.world_body_data.getWorldBody(source.world_body_id);
				const glm::dvec3 local_camera_position = glm::conjugate(body.rotation)
					* (world.camera.getWorldPosition() - body.position);
				const double height = world.planets.getPlanetHeightAboveSurface(
					source.world_body_id, local_camera_position);
				const bool was_enabled = planet.enabled;
				if(planet.enabled)
				{
					if(height < lod_deactivation_height) planet.enabled = false;
				}
				else if(height > lod_activation_height) planet.enabled = true;
				if(planet.enabled != was_enabled)
				{
					if(planet.enabled)
						danikk_framework::formatLogInfo(
							"Planet LOD enabled: body=% height_above_surface=% m activation_threshold=% m",
							source.world_body_id, height, lod_activation_height);
					else
						danikk_framework::formatLogInfo(
							"Planet LOD disabled: body=% height_above_surface=% m deactivation_threshold=% m reason=below surface coverage range",
							source.world_body_id, height, lod_deactivation_height);
				}

				if(planet.enabled)
				{
					double thresholds[planet_lod_level_count - 1];
					for(uint8 threshold_index = 0; threshold_index < planet_lod_level_count - 1; threshold_index++)
					{
						const double minimum = source.chunk_load_radius * 4.0
							* static_cast<double>(threshold_index + 1);
						const double scaled = source.radius * level_altitude_ratios[threshold_index];
						thresholds[threshold_index] = glm::max(minimum, scaled);
						if(threshold_index > 0)
							thresholds[threshold_index] = glm::max(
								thresholds[threshold_index], thresholds[threshold_index - 1] * 1.5);
					}

					const bool had_desired_level = planet.has_desired_level;
					const uint8 previous_desired_level = planet.desired_level;
					if(!planet.has_desired_level)
					{
						planet.desired_level = 0;
						while(planet.desired_level < planet_lod_level_count - 1
							&& height >= thresholds[planet.desired_level]) planet.desired_level++;
						planet.has_desired_level = true;
					}
					else
					{
						while(planet.desired_level < planet_lod_level_count - 1
							&& height >= thresholds[planet.desired_level] * 1.1)
							planet.desired_level++;
						while(planet.desired_level > 0
							&& height < thresholds[planet.desired_level - 1] * 0.9)
							planet.desired_level--;
					}
					if(!had_desired_level || previous_desired_level != planet.desired_level)
						danikk_framework::formatLogInfo(
							"Planet LOD desired level changed: body=% level=% height_above_surface=% m",
							source.world_body_id, planet.desired_level, height);

					planet.show_local_patch = height < thresholds[0] * 1.1;

					const auto queue_level_requests = [&](uint8 level_index)
					{
						PlanetLodLevel& level = planet.levels[level_index];
						if(level.camera_local)
						{
							PlanetLodPatch& patch = level.patches[local_patch_index];
							const double patch_width = glm::max(source.chunk_load_radius * 4.0, 128.0);
							const double patch_grid_step = patch_width * 0.5;
							const glm::dvec3 camera_direction = glm::normalize(local_camera_position);
							const glm::dvec3 camera_surface_position = camera_direction
								* PlanetSystem::getPlanetSurfaceRadius(*planet.terrain_snapshot, camera_direction);
							const glm::ivec3 patch_coordinate = glm::ivec3(glm::round(
								camera_surface_position / patch_grid_step));
							const glm::dvec3 snapped_position = glm::dvec3(patch_coordinate) * patch_grid_step;
							const glm::dvec3 snapped_direction = glm::length(snapped_position) > 1e-9
								? glm::normalize(snapped_position) : camera_direction;
							const glm::dvec3 patch_origin = snapped_direction
								* PlanetSystem::getPlanetSurfaceRadius(*planet.terrain_snapshot, snapped_direction);
							if(!patch.ready || patch.grid_coordinate != patch_coordinate)
							{
								if(patch.grid_coordinate != patch_coordinate)
								{
									patch.positions.clear();
									patch.triangle_indices.clear();
									patch.triangle_material_ids.clear();
									patch.ready = false;
									patch.grid_coordinate = patch_coordinate;
								}
								patch.origin = patch_origin;
								ChunkMeshGenerationRequest request;
								request.source = ChunkMeshSource::planet_surface_lod;
								request.source_revision = source.terrain_revision;
								request.world_body_id = source.world_body_id;
								request.planet_generation_id = source.generation_id;
								request.terrain_revision = source.terrain_revision;
								request.lod_resolution = level.resolution;
								request.lod_face = local_patch_index;
								request.lod_level = level_index;
								request.chunk_width = patch_width;
								request.voxel_size = patch_width / level.resolution;
								request.grid_coordinate = patch_coordinate;
								request.chunk_position = patch_origin;
								request.planet_terrain = planet.terrain_snapshot;
								requests.pushMove(request);
							}
							return;
						}

						for(uint8 face = 0; face < global_face_patch_count; face++)
						{
							PlanetLodPatch& patch = level.patches[face];
							if(patch.ready) continue;
							ChunkMeshGenerationRequest request;
							request.source = ChunkMeshSource::planet_surface_lod;
							request.source_revision = source.terrain_revision;
							request.world_body_id = source.world_body_id;
							request.planet_generation_id = source.generation_id;
							request.terrain_revision = source.terrain_revision;
							request.lod_resolution = level.resolution;
							request.lod_face = face;
							request.lod_level = level_index;
							request.grid_coordinate = glm::ivec3(face, level_index, 0);
							request.voxel_size = 1.57079632679489661923
								* source.radius / level.resolution;
							request.planet_terrain = planet.terrain_snapshot;
							requests.pushMove(request);
						}
					};

					if(planet.show_local_patch) queue_level_requests(local_patch_index);
					if(planet.desired_level != local_patch_index)
						queue_level_requests(planet.desired_level);
					if(planet.desired_level == local_patch_index)
						queue_level_requests(1);

					const bool had_active_level = planet.has_active_level;
					const uint8 previous_active_level = planet.active_level;
					PlanetLodLevel& desired_level = planet.levels[planet.desired_level];
					if(isLevelReady(desired_level))
					{
						planet.active_level = planet.desired_level;
						planet.has_active_level = true;
					}
					else if(!planet.has_active_level)
					{
						for(uint8 candidate = 0; candidate < planet_lod_level_count; candidate++)
						{
							if(!isLevelReady(planet.levels[candidate])) continue;
							planet.active_level = candidate;
							planet.has_active_level = true;
							break;
						}
					}
					if(planet.has_active_level
						&& (!had_active_level || previous_active_level != planet.active_level))
						danikk_framework::formatLogInfo(
							"Planet LOD mesh selected: body=% level=% desired_level=%",
							source.world_body_id, planet.active_level, planet.desired_level);
				}
			}
			world.chunk_mesh_generation.replacePlanetLodRequests(
				source.world_body_id, source.generation_id, requests);
		}
	}

	/** @copydoc PlanetLodSystem::acceptGeneratedMesh */
	void PlanetLodSystem::acceptGeneratedMesh(const GeneratedChunkMesh& generated_mesh)
	{
		DSE_WATCH();
		if(generated_mesh.source != ChunkMeshSource::planet_surface_lod
			|| generated_mesh.lod_level >= planet_lod_level_count
			|| generated_mesh.lod_face >= planet_lod_face_count) return;
		for(PlanetLodPlanet& planet : planets)
		{
			if(planet.world_body_id != generated_mesh.world_body_id
				|| planet.planet_generation_id != generated_mesh.planet_generation_id
				|| planet.terrain_revision != generated_mesh.terrain_revision) continue;
			PlanetLodLevel& level = planet.levels[generated_mesh.lod_level];
			if(level.resolution != generated_mesh.lod_resolution) return;
			if(generated_mesh.lod_face >= level.patches.size()) return;
			PlanetLodPatch& patch = level.patches[generated_mesh.lod_face];
			if(level.camera_local && patch.grid_coordinate != generated_mesh.grid_coordinate) return;
			patch.positions = generated_mesh.point_positions;
			patch.triangle_indices = generated_mesh.triangle_point_indices;
			patch.triangle_material_ids = generated_mesh.triangle_material_ids;
			patch.origin = generated_mesh.chunk_position;
			patch.ready = true;
			bool all_faces_ready = true;
			for(const PlanetLodPatch& face_patch : level.patches)
				if(!face_patch.ready) all_faces_ready = false;
			if(all_faces_ready)
			{
				uploadLevelMesh(level);
				level.initialized = true;
				for(PlanetLodPatch& face_patch : level.patches)
				{
					face_patch.positions = DynamicArray<vec3>();
					face_patch.triangle_indices = DynamicArray<uint32>();
					face_patch.triangle_material_ids = DynamicArray<MaterialId>();
				}
			}
			if(planet.desired_level == generated_mesh.lod_level
				&& isLevelReady(level))
			{
				planet.active_level = generated_mesh.lod_level;
				planet.has_active_level = true;
			}
			return;
		}
	}

	/** @copydoc PlanetLodSystem::drawFar */
	void PlanetLodSystem::drawFar() const
	{
		DSE_WATCH();
		if(!world.camera.isActive()) return;
		for(const PlanetLodPlanet& planet : planets)
		{
			if(!planet.enabled || !planet.has_active_level) continue;
			if(!world.world_body_data.isWorldBodyActive(planet.world_body_id)) continue;
			const WorldBody& body = world.world_body_data.getWorldBody(planet.world_body_id);
			setWorldMatrix(createPlanetTransform(world, body));
			uint8 global_level_index = planet.active_level;
			if(global_level_index == local_patch_index || !isLevelReady(planet.levels[global_level_index]))
			{
				global_level_index = planet_lod_level_count;
				for(uint8 candidate = 1; candidate < planet_lod_level_count; candidate++)
				{
					if(!isLevelReady(planet.levels[candidate])) continue;
					global_level_index = candidate;
					break;
				}
				if(global_level_index >= planet_lod_level_count) continue;
			}
			const PlanetLodLevel& level = planet.levels[global_level_index];
			if(level.mesh == nullptr) continue;
			for(const PlanetLodMaterialRange& range : level.material_ranges)
			{
				if(range.index_count == 0) continue;
				MaterialDefinition& material = getMaterial(range.material_id);
				if(material.texture.isNull()) white_texture.bind();
				else material.texture.bind();
				setDrawColor(vec4(material.color, 1.0f));
				level.mesh->drawRange(range.first_index, range.index_count);
			}
		}
	}

	/** @copydoc PlanetLodSystem::drawNear */
	void PlanetLodSystem::drawNear() const
	{
		DSE_WATCH();
		if(!world.camera.isActive()) return;
		for(const PlanetLodPlanet& planet : planets)
		{
			if(!planet.enabled || !planet.show_local_patch
				|| !world.world_body_data.isWorldBodyActive(planet.world_body_id)) continue;
			const PlanetLodLevel& level = planet.levels[local_patch_index];
			if(!level.initialized || level.mesh == nullptr) continue;
			const WorldBody& body = world.world_body_data.getWorldBody(planet.world_body_id);
			setWorldMatrix(createPlanetTransform(world, body, level.mesh_origin));
			for(const PlanetLodMaterialRange& range : level.material_ranges)
			{
				if(range.index_count == 0) continue;
				MaterialDefinition& material = getMaterial(range.material_id);
				if(material.texture.isNull()) white_texture.bind();
				else material.texture.bind();
				setDrawColor(vec4(material.color, 1.0f));
				level.mesh->drawRange(range.first_index, range.index_count);
			}
		}
	}

	/** @copydoc PlanetLodSystem::clear */
	void PlanetLodSystem::clear()
	{
		DSE_WATCH();
		DynamicArray<ChunkMeshGenerationRequest> no_requests;
		for(PlanetLodPlanet& planet : planets)
		{
			world.chunk_mesh_generation.replacePlanetLodRequests(
				planet.world_body_id, planet.planet_generation_id, no_requests);
			destroyLevelMeshes(planet);
		}
		planets.clear();
	}
}
