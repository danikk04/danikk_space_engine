#include <danikk_space_engine/profiler.h>
#include <danikk_space_engine/assets/collection.h>
#include <danikk_space_engine/component/point/human/human.h>
#include <danikk_space_engine/effects/unit_activity_labels.h>
#include <danikk_space_engine/factions.h>
#include <danikk_space_engine/unit_system.h>
#include <danikk_space_engine/world.h>

#include <danikk_engine/mesh.h>
#include <danikk_engine/texture_methods.h>

namespace danikk_space_engine
{
	namespace
	{
		/** @brief Returns the text shown for one hierarchical unit state. */
		const char* getStateText(UnitGlobalState global_state, UnitSubstate substate)
		{
			switch(global_state)
			{
				case UnitGlobalState::war:
					switch(substate)
					{
						case UnitSubstate::pursuit: return "PURSUIT";
						case UnitSubstate::attack: return "ATTACK";
						case UnitSubstate::reconnaissance: return "RECON";
						default: return "WAR";
					}
				case UnitGlobalState::work:
					switch(substate)
					{
						case UnitSubstate::moving_to_work: return "TO WORK";
						case UnitSubstate::collecting_corpses: return "CORPSES";
						default: return "WORK";
					}
				case UnitGlobalState::rest:
					return substate == UnitSubstate::sleep ? "SLEEP" : "IDLE";
			}
			return "IDLE";
		}

		/** @brief Counts bytes in the fixed ASCII activity labels. */
		size_t getTextLength(const char* text)
		{
			size_t length = 0;
			while(text[length] != '\0') ++length;
			return length;
		}

		/** @brief Adds one camera-facing quad to the shared background mesh. */
		void appendBillboardQuad(DynamicArray<DefaultVertex>& vertices,
			DynamicArray<gl_point_index_t>& indices, const vec3& center, const vec3& right,
			const vec3& up, const vec3& normal, float width, float height)
		{
			const vec3 half_right = right * (width * 0.5f);
			const vec3 half_up = up * (height * 0.5f);
			const gl_point_index_t first = static_cast<gl_point_index_t>(vertices.size());
			vertices.pushCtor(center - half_right - half_up, normal, vec2(0.0f));
			vertices.pushCtor(center + half_right - half_up, normal, vec2(0.0f));
			vertices.pushCtor(center + half_right + half_up, normal, vec2(0.0f));
			vertices.pushCtor(center - half_right + half_up, normal, vec2(0.0f));
			const gl_point_index_t quad[] = {
				first, first + 1, first + 2, first, first + 2, first + 3};
			for(gl_point_index_t index : quad) indices.push(index);
		}
	}

	/** @brief Draws batched camera-facing 3D activity labels above living allied units. */
	void drawUnitActivityLabels(World& world)
	{
		DSE_WATCH();
		static FontMesh text_mesh;
		static Mesh panel_mesh;
		static DynamicArray<DefaultVertex> panel_vertices;
		static DynamicArray<gl_point_index_t> panel_indices;
		text_mesh.clear();
		panel_vertices.clear();
		panel_indices.clear();

		const glm::dvec3 camera_position = world.camera.getWorldPosition();
		const CameraSystem& camera = world.camera;
		const vec3 camera_front = glm::normalize(camera.getFront());
		const vec3 camera_up = glm::normalize(camera.getUp());
		const vec3 camera_right = glm::normalize(glm::cross(camera_front, camera_up));
		const vec3 billboard_normal = glm::normalize(glm::cross(camera_right, camera_up));
		const mat4 view = glm::lookAt(vec3(0.0f), camera_front, camera_up);
		const mat4 projection = glm::perspective(glm::radians(camera.vertical_fov_degrees),
			screen_ratio_gz, camera.near_clip_distance, camera.far_clip_distance);
		constexpr vec2 glyph_size(0.075f, 0.12f);
		constexpr float glyph_spacing = 0.012f;
		constexpr float panel_padding_x = 0.05f;
		constexpr float panel_height = 0.18f;

		for(PointComponentSlotId slot_id : world.point_data.getUsedPointComponentSlotIds())
		{
			if(!world.point_data.isPointComponentSlotActive(slot_id)) continue;
			const PointComponentSlot& slot = world.point_data.getPointComponentSlot(slot_id);
			if(slot.type_id != PointComponentTypeId::human || !world.point_data.isPointActive(slot.point_id)
				|| !world.chunks.isChunkActive(slot.chunk_id)) continue;
			const HumanComponent& human = *static_cast<const HumanComponent*>(slot.getData());
			if(human.hasDeathTimestamp() || human.faction_id >= world.factions.size()
				|| world.factions.getRelation(base_faction_ids::player, human.faction_id).type
					!= FactionRelationType::allied) continue;

			UnitGlobalState global_state;
			UnitSubstate substate;
			if(!getUnitState(slot_id, slot.generation_id, global_state, substate)) continue;
			const char* label = getStateText(global_state, substate);
			const size_t character_count = getTextLength(label);
			const float text_width = character_count * glyph_size.x
				+ (character_count - 1) * glyph_spacing;
			const float panel_width = text_width + panel_padding_x * 2.0f;

			const glm::dvec3 human_position = world.getPointWorldPosition(slot.chunk_id, slot.point_id);
			const Chunk& chunk = world.chunks.getChunk(slot.chunk_id);
			const WorldBody& body = world.world_body_data.getWorldBody(chunk.world_body_id);
			vec3 human_up = vec3(body.rotation * glm::dvec3(human.up));
			if(glm::length(human_up) <= 1e-6f) human_up = vec3(0.0f, 0.0f, 1.0f);
			human_up = glm::normalize(human_up);
			const glm::dvec3 label_world_position = human_position
				+ glm::dvec3(human_up) * (human.getHeight() * 0.5 + 0.16)
				+ glm::dvec3(camera_right) * 0.20
				+ glm::dvec3(billboard_normal) * 0.6;
			const vec3 camera_relative_position = vec3(label_world_position - camera_position);
			const vec4 clip_position = projection * view * vec4(camera_relative_position, 1.0f);
			if(clip_position.w <= 0.0f) continue;
			const vec3 ndc = vec3(clip_position) / clip_position.w;
			if(ndc.x < -1.0f || ndc.x > 1.0f || ndc.y < -1.0f || ndc.y > 1.0f
				|| ndc.z < -1.0f || ndc.z > 1.0f) continue;

			const vec3 label_center = camera_relative_position;
			appendBillboardQuad(panel_vertices, panel_indices, label_center, camera_right,
				camera_up, billboard_normal, panel_width, panel_height);
			static_asset_collection.base_font.append3DText(text_mesh, String(label),
				label_center + billboard_normal * 0.003f, camera_right, camera_up,
				glyph_size, glyph_spacing);
		}
		if(panel_indices.size() == 0) return;

		static_asset_collection.bullet_tracer_shader.use();
		setViewMatrix(view);
		setProjectionMatrix(projection);
		setWorldMatrix(mat4(1.0f));
		setDepthWriteEnabled(false);
		if(panel_indices.size() > 0)
		{
			panel_mesh.setData(reinterpret_cast<const float*>(panel_vertices.data()),
				panel_vertices.size() * sizeof(DefaultVertex) / sizeof(float),
				panel_indices.data(), panel_indices.size());
			DefaultVertex::setAttributes();
			white_texture.bind();
			setDrawColor(vec4(0.055f, 0.065f, 0.075f, 1.0f));
			panel_mesh.draw();
		}
		for(FontMeshBatch& batch : text_mesh.getBatches())
		{
			if(batch.geometry.empty()) continue;
			batch.geometry.setDataToMesh(batch.mesh);
			DefaultVertex::setAttributes();
			bindTexture(batch.atlas_texture_handle, 0);
			setDrawColor(vec4(0.92f, 0.94f, 0.97f, 1.0f));
			batch.mesh.draw();
		}
		setDepthWriteEnabled(true);
		static_asset_collection.base3d_shader.use(true);
	}
}
