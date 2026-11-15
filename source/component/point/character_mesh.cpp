#include <danikk_space_engine/component/point/character.h>

namespace danikk_space_engine
{
	namespace
	{
		// Builds six outward-wound quads with rectangles from the classic 64x64 skin layout.
		// Positions form a unit cube; dimensions and joint placement are applied when rendering.
		void loadPartMesh(Mesh& mesh, int u, int v, int width, int height, int depth)
		{
			const vec3 normals[] = {
				vec3(0, 1, 0), vec3(0, -1, 0), vec3(1, 0, 0),
				vec3(-1, 0, 0), vec3(0, 0, 1), vec3(0, 0, -1)};
			const vec3 rights[] = {
				vec3(-1, 0, 0), vec3(1, 0, 0), vec3(0, 1, 0),
				vec3(0, -1, 0), vec3(1, 0, 0), vec3(1, 0, 0)};
			const vec3 ups[] = {
				vec3(0, 0, 1), vec3(0, 0, 1), vec3(0, 0, 1),
				vec3(0, 0, 1), vec3(0, 1, 0), vec3(0, -1, 0)};
			const glm::ivec4 rectangles[] = {
				{u + depth, v + depth, width, height},
				{u + depth * 2 + width, v + depth, width, height},
				{u, v + depth, depth, height},
				{u + depth + width, v + depth, depth, height},
				{u + depth, v, width, depth},
				{u + depth + width, v, width, depth}};
			const vec2 corners[] = {vec2(0, 0), vec2(1, 0), vec2(1, 1), vec2(0, 1)};
			DefaultVertex vertices[24];
			gl_point_index_t indices[36];
			for(uint32 face = 0; face < 6; face++)
			{
				const glm::ivec4& rect = rectangles[face];
				for(uint32 corner = 0; corner < 4; corner++)
				{
					const vec2 xy = corners[corner];
					const vec3 position = normals[face] * 0.5f
						+ rights[face] * (xy.x - 0.5f) + ups[face] * (xy.y - 0.5f);
					const vec2 uv((rect.x + xy.x * rect.z) / 64.0f,
						(rect.y + (1.0f - xy.y) * rect.w) / 64.0f);
					vertices[face * 4 + corner] = DefaultVertex(position, normals[face], uv);
				}
				const uint32 base = face * 4;
				const gl_point_index_t quad[] = {base, base + 1, base + 2, base, base + 2, base + 3};
				for(uint32 index = 0; index < 6; index++) indices[face * 6 + index] = quad[index];
			}
			mesh.setData(reinterpret_cast<const float*>(vertices), 24 * sizeof(DefaultVertex) / sizeof(float), indices, 36);
			DefaultVertex::setAttributes();
		}
	}

	// Loads shared meshes and defines proportions, pivots and fallback colours in units of body height.
	// Skin rectangles use separate left/right limb areas of a 64x64 texture, without the clothing layer.
	void loadCharacterDefinition(CharacterDefinition& definition)
	{
		const glm::ivec2 origins[] = {{0, 0}, {16, 16}, {40, 16}, {32, 48}, {0, 16}, {16, 48}};
		for(size_t index = 0; index < static_cast<size_t>(CharacterPart::count); index++)
		{
			CharacterPartDefinition& part = definition.parts[index];
			const bool head = index == static_cast<size_t>(CharacterPart::head);
			const bool torso = index == static_cast<size_t>(CharacterPart::torso);
			const int width = head || torso ? 8 : 4;
			const int height = head ? 8 : 12;
			const int depth = head ? 8 : 4;
			loadPartMesh(part.mesh, origins[index].x, origins[index].y, width, height, depth);
			part.size = vec3(width, depth, height) / 32.0f;
			part.pivot = vec3(0.0f, 0.0f, head || index < 4 ? 0.75f : 0.375f);
			part.center_offset = vec3(0.0f, 0.0f, head ? 0.125f : -0.1875f);
			if(index == 2 || index == 3) part.pivot.x = index == 2 ? 0.1875f : -0.1875f;
			if(index == 4 || index == 5) part.pivot.x = index == 4 ? 0.0625f : -0.0625f;
			part.color = head ? vec3(0.65f, 0.43f, 0.28f)
				: index < 4 ? vec3(0.15f, 0.35f, 0.45f) : vec3(0.12f, 0.15f, 0.26f);
		}
	}
}
