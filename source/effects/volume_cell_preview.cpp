#include <danikk_space_engine/effects/volume_cell_preview.h>

#include <danikk_space_engine/world/world.h>
#include <danikk_space_engine/asset.h>
#include <danikk_engine/texture_methods.h>

namespace danikk_space_engine
{
	// Builds and draws the same one-meter hexahedron used by Build -> Create VolumeCell.
	// Its base follows the raycast surface normal, and a small outward offset keeps the
	// preview visible against the hit surface until the cell is placed.
	void drawVolumeCellPlacementPreview(World& world, const RaycastHit& hit)
	{
		if(!hit.hit || !world.data.isChunkActive(hit.face.chunk_id)) return;

		const Chunk& chunk = world.data.getChunk(hit.face.chunk_id);
		const WorldBody& body = world.data.getWorldBody(chunk.world_body_id);
		const glm::dquat inverse_body_rotation = glm::conjugate(body.rotation);
		vec3 up = vec3(inverse_body_rotation * glm::dvec3(hit.normal));
		const float up_length = glm::length(up);
		if(up_length < 0.0001f) return;
		up /= up_length;
		const vec3 reference = glm::abs(glm::dot(up, vec3(0.0f, 0.0f, 1.0f))) > 0.9f
			? vec3(0.0f, 1.0f, 0.0f) : vec3(0.0f, 0.0f, 1.0f);
		const vec3 right = glm::normalize(glm::cross(reference, up));
		const vec3 forward = glm::normalize(glm::cross(up, right));
		const glm::dvec3 local_hit_position = inverse_body_rotation
			* (hit.position - world.getChunkWorldPosition(hit.face.chunk_id));
		const vec3 base_position = vec3(local_hit_position + glm::dvec3(up * 0.03f));
		constexpr float half_extent = 0.5f;
		constexpr float height = 1.0f;
		const vec3 local_vertices[8]
		{
			vec3(-half_extent, -half_extent, 0.0f),
			vec3(half_extent, -half_extent, 0.0f),
			vec3(half_extent, half_extent, 0.0f),
			vec3(-half_extent, half_extent, 0.0f),
			vec3(-half_extent, -half_extent, height),
			vec3(half_extent, -half_extent, height),
			vec3(half_extent, half_extent, height),
			vec3(-half_extent, half_extent, height)
		};
		vec3 positions[8];
		for(uint8 i = 0; i < 8; i++)
		{
			positions[i] = base_position
				+ right * local_vertices[i].x
				+ forward * local_vertices[i].y
				+ up * local_vertices[i].z;
		}

		const VolumeCellTopologyDefinition& topology = getVolumeCellTopologyDefinition(VolumeCellTopology::hexahedron);
		DefaultVertex vertices[36];
		gl_point_index_t indices[36];
		uint8 vertex_count = 0;
		for(uint8 triangle_index = 0; triangle_index < topology.triangle_count; triangle_index++)
		{
			const VolumeCellTriangle& triangle = topology.triangles[triangle_index];
			const vec3 a = positions[triangle.points[0]];
			const vec3 b = positions[triangle.points[1]];
			const vec3 c = positions[triangle.points[2]];
			const vec3 cross_product = glm::cross(b - a, c - a);
			const float normal_length = glm::length(cross_product);
			if(normal_length < 0.000001f) continue;
			const vec3 normal = cross_product / normal_length;
			const vec3 triangle_positions[3] = {a, b, c};
			const vec2 uvs[3] = {vec2(0.0f, 0.0f), vec2(1.0f, 0.0f), vec2(0.0f, 1.0f)};
			for(uint8 corner = 0; corner < 3; corner++)
			{
				vertices[vertex_count] = DefaultVertex(triangle_positions[corner], normal, uvs[corner]);
				indices[vertex_count] = vertex_count;
				vertex_count++;
			}
		}
		if(vertex_count == 0) return;

		static Mesh preview_mesh;
		preview_mesh.setData(
			reinterpret_cast<const float*>(vertices),
			vertex_count * sizeof(DefaultVertex) / sizeof(float),
			indices,
			vertex_count);
		DefaultVertex::setAttributes();
		white_texture.bind();
		setDrawColor(vec4(0.08f, 0.35f, 1.0f, 0.55f));
		setWorldMatrix(chunk.createWorldMatrix(world));
		preview_mesh.draw();
	}
}
