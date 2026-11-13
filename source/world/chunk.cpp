#include <algorithm>

#include <danikk_space_engine/world/chunk.h>
#include <danikk_space_engine/world/world.h>

#include <danikk_engine/mesh.h>
#include <danikk_engine/texture_methods.h>
#include <danikk_framework/glm.h>

#include <cstdlib>
#include <limits>
#include <new>

namespace danikk_space_engine
{
	struct FaceReference
	{
		PointId point_ids[4]{};
		uint8 point_count = 0;
		size_t cell_index = 0;
		uint8 face_index = 0;
	};

	static bool faceReferenceLess(const FaceReference& left, const FaceReference& right)
	{
		if(left.point_count != right.point_count)
		{
			return left.point_count < right.point_count;
		}
		for(uint8 i = 0; i < left.point_count; i++)
		{
			if(left.point_ids[i] != right.point_ids[i])
			{
				return left.point_ids[i] < right.point_ids[i];
			}
		}
		return false;
	}

	static bool sameFaceReference(const FaceReference& left, const FaceReference& right)
	{
		if(left.point_count != right.point_count)
		{
			return false;
		}
		for(uint8 i = 0; i < left.point_count; i++)
		{
			if(left.point_ids[i] != right.point_ids[i])
			{
				return false;
			}
		}
		return true;
	}

	static vec2 projectToUV(const vec3& position, const vec3& normal)
	{
		const vec3 absolute_normal = glm::abs(normal);
		if(absolute_normal.x >= absolute_normal.y && absolute_normal.x >= absolute_normal.z)
		{
			return vec2(position.z, position.y);
		}
		if(absolute_normal.y >= absolute_normal.z)
		{
			return vec2(position.x, position.z);
		}
		return vec2(position.x, position.y);
	}

	Chunk::Chunk(Chunk&& other) noexcept
		: mesh(other.mesh), shared_face_masks(static_cast<DynamicArray<uint8>&&>(other.shared_face_masks)),
		position(other.position), volume_cell_ids(static_cast<DynamicArray<VolumeCellId>&&>(other.volume_cell_ids)),
		point_component_slot_ids(static_cast<DynamicArray<PointComponentSlotId>&&>(other.point_component_slot_ids))
	{
		other.mesh = nullptr;
	}

	Chunk& Chunk::operator=(Chunk&& other) noexcept
	{
		if(this == &other)
		{
			return *this;
		}

		destroyMesh();
		position = other.position;
		volume_cell_ids = static_cast<DynamicArray<VolumeCellId>&&>(other.volume_cell_ids);
		point_component_slot_ids = static_cast<DynamicArray<PointComponentSlotId>&&>(other.point_component_slot_ids);
		shared_face_masks = static_cast<DynamicArray<uint8>&&>(other.shared_face_masks);
		mesh = other.mesh;
		other.mesh = nullptr;
		return *this;
	}

	Chunk::~Chunk()
	{
		destroyMesh();
	}

	void Chunk::destroyMesh()
	{
		if(mesh == nullptr)
		{
			return;
		}

		mesh->free();
		mesh->~Mesh();
		free(mesh);
		mesh = nullptr;
	}

	void Chunk::addPointComponentSlot(PointComponentSlotId slot_id)
	{
		point_component_slot_ids.push(slot_id);
	}

	void Chunk::tick(World& world)
	{
		const size_t component_count = point_component_slot_ids.size();
		for(size_t component_index = 0; component_index < component_count; component_index++)
		{
			const PointComponentSlotId slot_id = point_component_slot_ids[component_index];
			if(slot_id >= world.data.getPointComponentSlotCount())
			{
				danikk_engine::formatFatalError("Chunk references invalid point component slot %", slot_id);
			}

			PointComponentSlot& slot = world.data.getPointComponentSlot(slot_id);
			const PointComponentType& type = getPointComponentType(slot.type_id);
			if(type.tick == nullptr)
			{
				danikk_engine::formatFatalError(
					"Point component type % has no tick function",
					static_cast<uint32>(slot.type_id));
			}
			if(slot.uses_external_data && slot.external_data == nullptr)
			{
				danikk_engine::formatFatalError("Point component slot % has no external data", slot_id);
			}
			type.tick(slot, world);
		}
	}

	void Chunk::frame(World& world)
	{
		const size_t component_count = point_component_slot_ids.size();
		for(size_t component_index = 0; component_index < component_count; component_index++)
		{
			const PointComponentSlotId slot_id = point_component_slot_ids[component_index];
			if(slot_id >= world.data.getPointComponentSlotCount())
			{
				danikk_engine::formatFatalError("Chunk references invalid point component slot %", slot_id);
			}

			PointComponentSlot& slot = world.data.getPointComponentSlot(slot_id);
			if(slot.uses_external_data && slot.external_data == nullptr)
			{
				danikk_engine::formatFatalError("Point component slot % has no external data", slot_id);
			}

			const PointComponentType& type = getPointComponentType(slot.type_id);
			if(type.frame != nullptr)
			{
				type.frame(slot, world);
			}
		}
	}

	// Builds a triangle mesh for all volume cells owned by this chunk.
	// Sorts canonicalized face point IDs to identify shared faces in O(F log F), then emits
	// only visible triangles, reserves the output buffers, uploads them, and sets attributes.
	Mesh& Chunk::createMesh()
	{
		if(mesh == nullptr)
		{
			void* memory = malloc(sizeof(Mesh));
			if(memory == nullptr) std::abort();
			mesh = new (memory) Mesh();
		}

		DynamicArray<DefaultVertex> vertices;
		DynamicArray<gl_point_index_t> indices;
		DynamicArray<FaceReference> face_references;
		shared_face_masks.clear();
		shared_face_masks.reserve(volume_cell_ids.size());
		face_references.reserve(volume_cell_ids.size() * 8);
		size_t max_triangle_count = 0;

		for(size_t cell_index = 0; cell_index < volume_cell_ids.size(); cell_index++)
		{
			const VolumeCellId cell_id = volume_cell_ids[cell_index];
			if(cell_id >= world.data.getVolumeCellCount())
			{
				std::abort();
			}
			const VolumeCell& cell = world.data.getVolumeCell(cell_id);
			const VolumeCellTopologyDefinition& topology = getVolumeCellTopologyDefinition(cell.topology);
			shared_face_masks.push(0);
			max_triangle_count += topology.triangle_count;
			for(uint8 face_index = 0; face_index < topology.face_count; face_index++)
			{
				FaceReference reference;
				reference.point_count = getVolumeCellFacePointIds(cell, face_index, reference.point_ids);
				reference.cell_index = cell_index;
				reference.face_index = face_index;
				for(uint8 point_index = 1; point_index < reference.point_count; point_index++)
				{
					const PointId point_id = reference.point_ids[point_index];
					uint8 insert_index = point_index;
					while(insert_index > 0 && reference.point_ids[insert_index - 1] > point_id)
					{
						reference.point_ids[insert_index] = reference.point_ids[insert_index - 1];
						insert_index--;
					}
					reference.point_ids[insert_index] = point_id;
				}
				face_references.push(reference);
			}
		}

		std::sort(face_references.begin(), face_references.end(), faceReferenceLess);
		for(size_t face_index = 0; face_index < face_references.size();)
		{
			size_t group_end = face_index + 1;
			while(group_end < face_references.size()
				&& sameFaceReference(face_references[face_index], face_references[group_end]))
			{
				group_end++;
			}
			if(group_end - face_index > 1)
			{
				for(size_t reference_index = face_index; reference_index < group_end; reference_index++)
				{
					const FaceReference& reference = face_references[reference_index];
					shared_face_masks[reference.cell_index] |= static_cast<uint8>(1u << reference.face_index);
				}
			}
			face_index = group_end;
		}

		vertices.reserve(max_triangle_count * 3);
		indices.reserve(max_triangle_count * 3);
		for(size_t cell_index = 0; cell_index < volume_cell_ids.size(); cell_index++)
		{
			const VolumeCellId cell_id = volume_cell_ids[cell_index];
			const VolumeCell& cell = world.data.getVolumeCell(cell_id);
			const VolumeCellTopologyDefinition& topology = getVolumeCellTopologyDefinition(cell.topology);
			const uint8 shared_face_mask = shared_face_masks[cell_index];
			for(uint8 triangle_index = 0; triangle_index < topology.triangle_count; triangle_index++)
			{
				if((shared_face_mask & static_cast<uint8>(1u << topology.triangle_face_indices[triangle_index])) != 0)
				{
					continue;
				}
				const VolumeCellTriangle& triangle = topology.triangles[triangle_index];
				const PointId point_ids[3]
				{
					cell.points[triangle.points[0]],
					cell.points[triangle.points[1]],
					cell.points[triangle.points[2]]
				};

				if(point_ids[0] >= world.data.getPointCount() || point_ids[1] >= world.data.getPointCount() || point_ids[2] >= world.data.getPointCount())
				{
					std::abort();
				}

				const vec3 a = world.data.getPoint(point_ids[0]).position;
				const vec3 b = world.data.getPoint(point_ids[1]).position;
				const vec3 c = world.data.getPoint(point_ids[2]).position;
				const vec3 cross_product = glm::cross(b - a, c - a);
				const float normal_length = glm::length(cross_product);
				if(normal_length <= 0.000001f)
				{
					continue;
				}

				if(vertices.size() + 3 > static_cast<size_t>(std::numeric_limits<gl_point_index_t>::max()) + 1)
				{
					std::abort();
				}

				const vec3 normal = cross_product / normal_length;
				const vec3 triangle_positions[3] = {a, b, c};
				const vec2 triangle_uvs[3] =
				{
					projectToUV(a, normal),
					projectToUV(b, normal),
					projectToUV(c, normal)
				};

				for(uint8 vertex_index = 0; vertex_index < 3; vertex_index++)
				{
					indices.push(static_cast<gl_point_index_t>(vertices.size()));
					vertices.pushCtor(triangle_positions[vertex_index], normal, triangle_uvs[vertex_index]);
				}
			}
		}

		mesh->setData(
			reinterpret_cast<const float*>(vertices.data()),
			vertices.size() * sizeof(DefaultVertex) / sizeof(float),
			indices.data(),
			indices.size());
		DefaultVertex::setAttributes();
		return *mesh;
	}

	bool Chunk::hasSharedFace(VolumeCellId cell_id, uint8 face_index) const
	{
		if(cell_id >= world.data.getVolumeCellCount())
		{
			return false;
		}
		const VolumeCell& cell = world.data.getVolumeCell(cell_id);
		for(VolumeCellId other_cell_id : volume_cell_ids)
		{
			if(other_cell_id == cell_id || other_cell_id >= world.data.getVolumeCellCount())
			{
				continue;
			}
			const VolumeCell& other_cell = world.data.getVolumeCell(other_cell_id);
			const VolumeCellTopologyDefinition& other_topology = getVolumeCellTopologyDefinition(other_cell.topology);
			for(uint8 other_face = 0; other_face < other_topology.face_count; other_face++)
			{
				if(volumeCellFacesSharePoints(cell, face_index, other_cell, other_face))
				{
					return true;
				}
			}
		}
		return false;
	}

	uint8 Chunk::getSharedFaceMask(size_t cell_index) const
	{
		return cell_index < shared_face_masks.size() ? shared_face_masks[cell_index] : 0;
	}

	mat4 Chunk::createWorldMatrix() const
	{
		return glm::translate(mat4(1.0f), position);
	}

	void Chunk::draw()
	{
		if(mesh == nullptr || volume_cell_ids.size() == 0)
		{
			return;
		}

		const VolumeCellId cell_id = volume_cell_ids[0];
		if(cell_id >= world.data.getVolumeCellCount())
		{
			return;
		}
		MaterialDefinition& material = getMaterial(world.data.getVolumeCell(cell_id).material);
		if(material.texture.isNull())
		{
			white_texture.bind();
		}
		else
		{
			material.texture.bind();
		}

		setDrawColor(vec4(material.color, 1.0f));
		setWorldMatrix(createWorldMatrix());
		mesh->draw();
	}
}
