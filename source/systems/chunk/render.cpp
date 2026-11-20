#include <cstdlib>
#include <limits>
#include <memory>
#include <new>

#include <danikk_space_engine/profiler.h>
#include <danikk_space_engine/systems/chunk/manager.h>
#include <danikk_space_engine/world.h>
#include <danikk_space_engine/config.h>

#include <danikk_engine/danikk_engine.h>
#include <danikk_engine/texture_methods.h>
#include <danikk_framework/glm.h>
#include <danikk_framework/log.h>

namespace danikk_space_engine
{
	namespace
	{
		/** @brief Projects a chunk-local vertex onto a planar UV basis chosen by its face normal. */
		vec2 projectToUV(const vec3& position, const vec3& normal)
		{
			DSE_WATCH();
			const vec3 absolute_normal = glm::abs(normal);
			if(absolute_normal.x >= absolute_normal.y && absolute_normal.x >= absolute_normal.z)
				return vec2(position.z, position.y);
			if(absolute_normal.y >= absolute_normal.z) return vec2(position.x, position.z);
			return vec2(position.x, position.y);
		}
	}

	ChunkRenderRecord& ChunkSystem::getRenderRecord(ChunkId chunk_id)
	{
		DSE_WATCH();
		if(render_records.size() <= chunk_id) render_records.resize(static_cast<size_t>(chunk_id) + 1);
		return render_records[chunk_id];
	}

	const ChunkRenderRecord* ChunkSystem::findRenderRecord(ChunkId chunk_id) const
	{
		DSE_WATCH();
		return chunk_id < render_records.size() ? &render_records[chunk_id] : nullptr;
	}

	/**
	 * @brief Converts transient indexed triangles into a flat-shaded, material-grouped GPU mesh.
	 *
	 * The method validates input indices, duplicates vertices per triangle, derives UVs from each
	 * face normal, calculates local bounds for raycast broadphase, then uploads the arrays. CPU
	 * geometry remains owned by the caller and is not retained.
	 */
	void ChunkSystem::createMesh(Chunk& chunk, const vec3* positions, size_t position_count,
		const uint32* triangle_indices, size_t index_count,
		const MaterialId* triangle_material_ids, size_t material_count,
		bool is_planet_terrain, bool is_build_part_mesh)
	{
		DSE_WATCH();
		if(index_count % 3 != 0 || material_count != index_count / 3
			|| (index_count > 0 && (positions == nullptr || triangle_indices == nullptr || triangle_material_ids == nullptr)))
			danikk_engine::formatFatalError("Invalid temporary chunk mesh buffer");

		ChunkRenderRecord& record = getRenderRecord(chunk.chunk_id);
		if(record.mesh == nullptr)
		{
			void* memory = malloc(sizeof(Mesh));
			if(memory == nullptr) std::abort();
			record.mesh = new (memory) Mesh();
		}

		DynamicArray<DefaultVertex> vertices;
		DynamicArray<gl_point_index_t> indices;
		record.material_ranges.clear();
		vertices.reserve(index_count);
		indices.reserve(index_count);

		for(MaterialId material_id = 0; material_id < getMaterialCount(); material_id++)
		{
			const uint32 first_index = static_cast<uint32>(indices.size());
			for(size_t triangle = 0; triangle < material_count; triangle++)
			{
				if(triangle_material_ids[triangle] != material_id) continue;
				const uint32 source_indices[3] = {
					triangle_indices[triangle * 3], triangle_indices[triangle * 3 + 1], triangle_indices[triangle * 3 + 2]};
				if(source_indices[0] >= position_count || source_indices[1] >= position_count || source_indices[2] >= position_count)
					danikk_engine::formatFatalError("Generated chunk mesh has an invalid vertex index");
				const vec3& a = positions[source_indices[0]];
				const vec3& b = positions[source_indices[1]];
				const vec3& c = positions[source_indices[2]];
				const vec3 cross_product = glm::cross(b - a, c - a);
				const float normal_length = glm::length(cross_product);
				if(normal_length <= 0.000001f) continue;
				if(vertices.size() + 3 > static_cast<size_t>(std::numeric_limits<gl_point_index_t>::max()) + 1)
					std::abort();
				const vec3 normal = cross_product / normal_length;
				const vec3 triangle_positions[3] = {a, b, c};
				for(const vec3& position : triangle_positions)
				{
					indices.push(static_cast<gl_point_index_t>(vertices.size()));
					vertices.pushCtor(position, normal, projectToUV(position, normal));
				}
			}
			const uint32 range_index_count = static_cast<uint32>(indices.size()) - first_index;
			if(range_index_count > 0)
				record.material_ranges.push(ChunkMaterialRange{material_id, first_index, range_index_count});
		}

		if(vertices.size() == 0)
		{
			chunk.bounds_center = vec3(0.0f);
			chunk.bounds_radius = 0.0f;
			chunk.bounds_valid = false;
		}
		else
		{
			vec3 minimum = vertices[0].pos;
			vec3 maximum = vertices[0].pos;
			for(const DefaultVertex& vertex : vertices)
			{
				minimum = glm::min(minimum, vertex.pos);
				maximum = glm::max(maximum, vertex.pos);
			}
			chunk.bounds_center = (minimum + maximum) * 0.5f;
			chunk.bounds_radius = 0.0f;
			for(const DefaultVertex& vertex : vertices)
				chunk.bounds_radius = glm::max(chunk.bounds_radius, glm::length(vertex.pos - chunk.bounds_center));
			chunk.bounds_radius += 0.0001f;
			chunk.bounds_valid = true;
		}

		record.mesh->setData(
			reinterpret_cast<const float*>(vertices.data()),
			vertices.size() * sizeof(DefaultVertex) / sizeof(float), indices.data(), indices.size());
		DefaultVertex::setAttributes();
		chunk.planet_terrain = is_planet_terrain;
		record.has_build_part_mesh = is_build_part_mesh;
	}

	/** @brief Draws each non-empty material range using the owning chunk's transform. */
	ChunkDrawResult ChunkSystem::draw(const Chunk& chunk)
	{
		DSE_WATCH();
		const ChunkRenderRecord* record = findRenderRecord(chunk.chunk_id);
		if(record == nullptr || record->mesh == nullptr) return ChunkDrawResult::missing_mesh;
		if(record->material_ranges.size() == 0) return ChunkDrawResult::empty_mesh;
		setWorldMatrix(chunk.createWorldMatrix(world));
		bool drew_range = false;
		for(const ChunkMaterialRange& range : record->material_ranges)
		{
			if(range.index_count == 0) continue;
			MaterialDefinition& material = getMaterial(range.material_id);
			if(material.texture.isNull()) white_texture.bind();
			else material.texture.bind();
			setDrawColor(vec4(material.color, 1.0f));
			record->mesh->drawRange(range.first_index, range.index_count);
			drew_range = true;
		}
		return drew_range ? ChunkDrawResult::drawn : ChunkDrawResult::empty_mesh;
	}

	/** @copydoc ChunkSystem::renderWorld */
	void ChunkSystem::renderWorld()
	{
		DSE_WATCH();
		const glm::dvec3 camera_position = world.camera.getWorldPosition();
		double far_render_distance = world.camera.far_clip_distance;
		double closest_planet_surface_distance = std::numeric_limits<double>::max();
		for(const PlanetData& planet : world.planets.getPlanets())
		{
			if(!world.world_body_data.isWorldBodyActive(planet.world_body_id)) continue;
			const PlanetSettings& planet_settings = getPlanetSettings(planet.settings_id);
			const WorldBody& body = world.world_body_data.getWorldBody(planet.world_body_id);
			const glm::dvec3 local_camera_position = glm::conjugate(body.rotation)
				* (camera_position - body.position);
			const double surface_height = world.planets.getPlanetHeightAboveSurface(
				planet.world_body_id, local_camera_position);
			closest_planet_surface_distance = glm::min(
				closest_planet_surface_distance, std::abs(surface_height));
			far_render_distance = glm::max(far_render_distance,
				glm::length(body.position - camera_position) + planet_settings.radius);
		}
		for(WorldBodyId body_id : world.world_body_data.getUsedWorldBodyIds())
		{
			const WorldBody& body = world.world_body_data.getWorldBody(body_id);
			for(ChunkId chunk_id : body.chunk_ids)
			{
				if(!isChunkActive(chunk_id)) continue;
				const Chunk& chunk = getChunk(chunk_id);
				if(!chunk.hasValidBounds()) continue;
				const glm::dvec3 chunk_center = body.position
					+ body.rotation * (chunk.position + glm::dvec3(chunk.bounds_center));
				far_render_distance = glm::max(far_render_distance,
					glm::length(chunk_center - camera_position) + chunk.bounds_radius);
			}
		}

		const auto draw_chunk = [this](ChunkId chunk_id)
		{
			render_candidates++;
			if(!isChunkActive(chunk_id))
			{
				render_inactive++;
				return;
			}
			switch(draw(getChunk(chunk_id)))
			{
				case ChunkDrawResult::missing_mesh: render_missing_mesh++; break;
				case ChunkDrawResult::empty_mesh: render_empty_mesh++; break;
				case ChunkDrawResult::drawn: render_drawn++; break;
			}
		};
		const auto draw_chunk_meshes = [this, &draw_chunk]()
		{
			for(WorldBodyId body_id : world.world_body_data.getUsedWorldBodyIds())
			{
				const WorldBody& body = world.world_body_data.getWorldBody(body_id);
				if(world.planets.isPlanet(body_id))
				{
					if(world.planets.shouldDrawPlanetChunks(body_id))
					{
						const DynamicArray<ChunkId>* regular_chunk_ids = world.planets.getRegularChunkIds(body_id);
						if(regular_chunk_ids != nullptr)
							for(ChunkId chunk_id : *regular_chunk_ids) draw_chunk(chunk_id);
					}
					else
					{
						for(ChunkId chunk_id : body.chunk_ids)
						{
							if(!isChunkActive(chunk_id)) { draw_chunk(chunk_id); continue; }
							if(!getChunk(chunk_id).isPlanetTerrain()) draw_chunk(chunk_id);
						}
					}
					continue;
				}
				for(ChunkId chunk_id : body.chunk_ids) draw_chunk(chunk_id);
			}
		};

		if(far_render_distance > world.camera.far_clip_distance * 1.05)
		{
			float far_near_clip = world.camera.far_clip_distance * 0.95f;
			if(closest_planet_surface_distance < std::numeric_limits<double>::max())
				far_near_clip = glm::min(far_near_clip, static_cast<float>(glm::max(
					static_cast<double>(world.camera.near_clip_distance), closest_planet_surface_distance * 0.5)));
			const float far_clip = static_cast<float>(glm::min(
				far_render_distance * 1.02, static_cast<double>(std::numeric_limits<float>::max() / 2.0f)));
			setProjectionMatrix(glm::perspective(glm::radians(world.camera.vertical_fov_degrees),
				screen_ratio_gz, far_near_clip, far_clip));
			setDepthRange(0.5f, 1.0f);
			world.planets.frame(PlanetRenderPass::far_depth);
			draw_chunk_meshes();

			world.camera.frame();
			setDepthRange(0.0f, 0.5f);
			world.planets.frame(PlanetRenderPass::near_depth);
			draw_chunk_meshes();
		}
		else
		{
			setDepthRange(0.0f, 1.0f);
			world.planets.frame(PlanetRenderPass::all);
			draw_chunk_meshes();
		}

		const uint32 now = danikk_engine::getMsTimestamp();
		if(render_report_timestamp == 0) render_report_timestamp = now;
		if(now - render_report_timestamp >= 1000)
		{
			danikk_framework::formatLogInfo(
				"Chunk rendering (last 1 s): references=% drawn=% missing_mesh=% empty_mesh=% inactive_ids=%",
				render_candidates, render_drawn, render_missing_mesh, render_empty_mesh, render_inactive);
			render_report_timestamp = now;
			render_candidates = 0;
			render_drawn = 0;
			render_missing_mesh = 0;
			render_empty_mesh = 0;
			render_inactive = 0;
		}
		setDepthRange(0.0f, 1.0f);
	}

	/** @copydoc ChunkSystem::destroyRenderData */
	void ChunkSystem::destroyRenderData(Chunk& chunk)
	{
		DSE_WATCH();
		const ChunkId chunk_id = chunk.chunk_id;
		if(chunk_id < render_records.size())
		{
			ChunkRenderRecord& record = render_records[chunk_id];
			if(record.mesh != nullptr)
			{
				record.mesh->free();
				std::destroy_at(record.mesh);
				free(record.mesh);
				record.mesh = nullptr;
			}
			record.material_ranges.clear();
			record.has_build_part_mesh = false;
		}
		chunk.bounds_valid = false;
		chunk.bounds_center = vec3(0.0f);
		chunk.bounds_radius = 0.0f;
		chunk.planet_terrain = false;
	}

	/** @copydoc ChunkSystem::clearRenderData */
	void ChunkSystem::clearRenderData()
	{
		DSE_WATCH();
		for(ChunkRenderRecord& record : render_records)
		{
			if(record.mesh == nullptr) continue;
			record.mesh->free();
			std::destroy_at(record.mesh);
			free(record.mesh);
			record.mesh = nullptr;
			record.material_ranges.clear();
			record.has_build_part_mesh = false;
		}
		render_records.clear();
	}

	bool ChunkSystem::hasBuildPartMesh(ChunkId chunk_id) const
	{
		DSE_WATCH();
		const ChunkRenderRecord* record = findRenderRecord(chunk_id);
		return record != nullptr && record->has_build_part_mesh;
	}
}
