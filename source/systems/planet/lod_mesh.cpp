#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <memory>
#include <new>

#include <danikk_space_engine/profiler.h>
#include <danikk_space_engine/systems/planet/system.h>
#include <danikk_space_engine/world.h>
#include <danikk_space_engine/material.h>
#include <danikk_space_engine/assets/collection.h>

#include <danikk_engine/texture_methods.h>
#include <danikk_framework/log.h>

namespace danikk_space_engine
{
	void PlanetSystem::init()
	{
		danikk_framework::formatLogInfo("Init PlanetSystem");
		clearLod();
	}
	void PlanetSystem::shutdown()
	{
		danikk_framework::formatLogInfo("Shutdown PlanetSystem");
		clearLod();
	}
	namespace
	{
vec2 projectLocalPositionToUv(const vec3& position, const vec3& normal)
		{
			const vec3 absolute_normal = glm::abs(normal);
			if(absolute_normal.x >= absolute_normal.y && absolute_normal.x >= absolute_normal.z)
				return vec2(position.z, position.y);
			if(absolute_normal.y >= absolute_normal.z) return vec2(position.x, position.z);
			return vec2(position.x, position.y);
		}

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

void uploadLevelMesh(PlanetChunkLevel& level)
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
						const vec3 local_position = vec3(planet_position - level.mesh_origin);
						const vec2 uv = level.camera_local
							? projectLocalPositionToUv(local_position, normal)
							: vec2(
								std::atan2(planet_position.y, planet_position.x)
									* planet_position_length / 32.0,
								std::asin(glm::clamp(static_cast<double>(radial_normal.z), -1.0, 1.0))
									* planet_position_length / 32.0);
						vertices.pushCtor(local_position, normal, uv);
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
			if(!level.camera_local) level.mesh_origin = glm::dvec3(0.0);
		}
	}

	void PlanetSystem::destroyPlanetLodMeshes(PlanetChunkRenderData& planet)
	{
		for(PlanetChunkLevel& level : planet.levels)
		{
			destroyMesh(level.mesh);
			level.material_ranges.clear();
			level.initialized = false;
			level.grid_center_set = false;
			level.grid_center = glm::ivec3(0);
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
		planet.lod_level = 0;
		planet.terrain_snapshot.reset();
	}

	/** @copydoc PlanetSystem::acceptGeneratedLodMesh */
	void PlanetSystem::acceptGeneratedLodMesh(const GeneratedChunkMesh& generated_mesh)
	{
		DSE_WATCH();
		if(generated_mesh.source != ChunkMeshSource::planet_surface_lod
			|| generated_mesh.lod_level == 0
			|| generated_mesh.lod_level >= planet_chunk_level_count) return;
		for(PlanetChunkRenderData& planet : chunk_render_data)
		{
			if(planet.world_body_id != generated_mesh.world_body_id
				|| planet.planet_generation_id != generated_mesh.planet_generation_id
				|| planet.terrain_revision != generated_mesh.terrain_revision) continue;
			PlanetChunkLevel& level = planet.levels[generated_mesh.lod_level];
			if(level.resolution != generated_mesh.lod_resolution) return;
			if(generated_mesh.lod_face >= level.patches.size()) return;
			PlanetLodPatch& patch = level.patches[generated_mesh.lod_face];
			if(patch.grid_coordinate != generated_mesh.grid_coordinate) return;
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
			return;
		}
	}

	/** @copydoc PlanetSystem::clearLod */
	void PlanetSystem::clearLod()
	{
		DSE_WATCH();
		DynamicArray<ChunkMeshGenerationRequest> no_requests;
		for(PlanetChunkRenderData& planet : chunk_render_data)
		{
			world.chunk_mesh_generation.replacePlanetLodRequests(
				planet.world_body_id, planet.planet_generation_id, no_requests);
			destroyPlanetLodMeshes(planet);
		}
		chunk_render_data.clear();
	}
}
