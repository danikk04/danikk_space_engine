#include <cstdlib>
#include <new>

#include <danikk_space_engine/profiler.h>
#include <danikk_space_engine/component/point/build_part.h>
#include <danikk_space_engine/asset.h>
#include <danikk_space_engine/world/world.h>

namespace danikk_space_engine
{
	namespace
	{
		Mesh* build_part_meshes[static_cast<size_t>(BuildPartShape::count)]{};

		/**
		 * @brief Lazily builds and returns the shared GPU mesh for a primitive shape.
		 * @param shape Registered BuildPart shape.
		 * @return Cached mesh uploaded from the shared local-space shape geometry.
		 */
		Mesh& getBuildPartMesh(BuildPartShape shape)
		{
			const size_t index = static_cast<size_t>(shape);
			if(index >= static_cast<size_t>(BuildPartShape::count))
			{
				danikk_engine::formatFatalError("Invalid BuildPart shape %", index);
			}
			if(build_part_meshes[index] != nullptr) return *build_part_meshes[index];

			const BuildPartGeometry& geometry = getBuildPartGeometry(shape);
			if(geometry.positions.size() == 0 || geometry.triangles.size() == 0)
			{
				danikk_engine::formatFatalError("BuildPart shape % has no geometry", index);
			}

			DynamicArray<DefaultVertex> vertices;
			DynamicArray<gl_point_index_t> indices;
			vertices.reserve(geometry.triangles.size() * 3);
			indices.reserve(geometry.triangles.size() * 3);
			for(const CubeTriangle& triangle : geometry.triangles)
			{
				const vec3 a = geometry.positions[triangle.points[0]];
				const vec3 b = geometry.positions[triangle.points[1]];
				const vec3 c = geometry.positions[triangle.points[2]];
				const vec3 normal = glm::normalize(glm::cross(b - a, c - a));
				const vec3 face_positions[3] = {a, b, c};
				for(const vec3& position : face_positions)
				{
					indices.push(static_cast<gl_point_index_t>(vertices.size()));
					vertices.pushCtor(position, normal, vec2(position.x + 0.5f, position.y + 0.5f));
				}
			}

			void* memory = malloc(sizeof(Mesh));
			if(memory == nullptr) std::abort();
			Mesh* mesh = new (memory) Mesh();
			mesh->setData(
				reinterpret_cast<const float*>(vertices.data()),
				vertices.size() * sizeof(DefaultVertex) / sizeof(float),
				indices.data(),
				indices.size());
			DefaultVertex::setAttributes();
			build_part_meshes[index] = mesh;
			return *mesh;
		}
	}

	/** @copydoc constructBuildPartComponent */
	void constructBuildPartComponent(void* destination, const void* init_data)
	{
		if(destination == nullptr)
		{
			danikk_engine::formatFatalError("BuildPart component construction received null destination");
		}
		new (destination) BuildPartComponent(init_data == nullptr
			? BuildPartComponent()
			: *static_cast<const BuildPartComponent*>(init_data));
	}

	/** @copydoc moveBuildPartComponent */
	void moveBuildPartComponent(void* destination, void* source)
	{
		if(destination == nullptr || source == nullptr)
		{
			danikk_engine::formatFatalError("BuildPart component move received null memory");
		}
		BuildPartComponent* source_component = static_cast<BuildPartComponent*>(source);
		new (destination) BuildPartComponent(*source_component);
		std::destroy_at(source_component);
	}

	/** @copydoc destroyBuildPartComponent */
	void destroyBuildPartComponent(void* data)
	{
		if(data == nullptr)
		{
			danikk_engine::formatFatalError("BuildPart component destruction received null memory");
		}
		std::destroy_at(static_cast<BuildPartComponent*>(data));
	}

	/** @copydoc frameBuildPartComponent */
	void frameBuildPartComponent(PointComponentSlot& slot, World& world)
	{
		DSE_WATCH();
		if(!world.data.isPointActive(slot.point_id)
			|| !world.data.isChunkActive(slot.chunk_id))
		{
			danikk_engine::formatFatalError("BuildPart component references an invalid anchor");
		}
		const BuildPartComponent& part = *static_cast<const BuildPartComponent*>(slot.getData());
		const Chunk& chunk = world.data.getChunk(slot.chunk_id);
		if(world.chunk_render_system.hasBuildPartMesh(chunk.chunk_id)) return;
		const mat4 world_matrix = chunk.createWorldMatrix(world)
			* glm::translate(mat4(1.0f), world.data.getPoint(slot.point_id).position)
			* part.rotation
			* glm::scale(mat4(1.0f), part.size);
		setWorldMatrix(world_matrix);
		white_texture.bind();
		setDrawColor(vec4(0.65f, 0.68f, 0.72f, 1.0f));
		getBuildPartMesh(part.shape).draw();
	}

	/** @copydoc clearBuildPartMeshCache */
	void clearBuildPartMeshCache()
	{
		for(Mesh*& mesh : build_part_meshes)
		{
			if(mesh == nullptr) continue;
			mesh->free();
			std::destroy_at(mesh);
			free(mesh);
			mesh = nullptr;
		}
	}
}
