#include <danikk_space_engine/world/chunk.h>
#include <danikk_space_engine/world/world.h>

#include <cstdlib>

#include <danikk_engine/mesh.h>
#include <danikk_engine/texture_methods.h>
#include <danikk_framework/glm.h>

namespace danikk_space_engine
{
	Chunk::Chunk(Chunk&& other) noexcept
		: mesh(other.mesh), shared_face_masks(static_cast<DynamicArray<uint8>&&>(other.shared_face_masks)),
		material_ranges(static_cast<DynamicArray<ChunkMaterialRange>&&>(other.material_ranges)),
		mesh_dirty(other.mesh_dirty), chunk_id(other.chunk_id), world_body_id(other.world_body_id),
		grid_coordinate(other.grid_coordinate), position(other.position),
		volume_cell_ids(static_cast<DynamicArray<VolumeCellId>&&>(other.volume_cell_ids)),
		ground_triangle_ids(static_cast<DynamicArray<GroundTriangleId>&&>(other.ground_triangle_ids)),
		point_ids(static_cast<DynamicArray<PointId>&&>(other.point_ids)),
		point_component_slot_ids(static_cast<DynamicArray<PointComponentSlotId>&&>(other.point_component_slot_ids)),
		graph_component_slot_ids(static_cast<DynamicArray<GraphComponentSlotId>&&>(other.graph_component_slot_ids))
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
		mesh_dirty = other.mesh_dirty;
		chunk_id = other.chunk_id;
		world_body_id = other.world_body_id;
		grid_coordinate = other.grid_coordinate;
		position = other.position;
		volume_cell_ids = static_cast<DynamicArray<VolumeCellId>&&>(other.volume_cell_ids);
		ground_triangle_ids = static_cast<DynamicArray<GroundTriangleId>&&>(other.ground_triangle_ids);
		point_ids = static_cast<DynamicArray<PointId>&&>(other.point_ids);
		point_component_slot_ids = static_cast<DynamicArray<PointComponentSlotId>&&>(other.point_component_slot_ids);
		graph_component_slot_ids = static_cast<DynamicArray<GraphComponentSlotId>&&>(other.graph_component_slot_ids);
		shared_face_masks = static_cast<DynamicArray<uint8>&&>(other.shared_face_masks);
		material_ranges = static_cast<DynamicArray<ChunkMaterialRange>&&>(other.material_ranges);
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
		if(mesh != nullptr)
		{
			mesh->free();
			mesh->~Mesh();
			free(mesh);
			mesh = nullptr;
		}
	}

	void Chunk::invalidateMesh()
	{
		mesh_dirty = true;
		shared_face_masks.clear();
		material_ranges.clear();
	}

	void Chunk::addPointComponentSlot(PointComponentSlotId slot_id)
	{
		point_component_slot_ids.push(slot_id);
	}

	void Chunk::addGraphComponentSlot(GraphComponentSlotId slot_id)
	{
		graph_component_slot_ids.push(slot_id);
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
			if(!world.data.isPointComponentSlotActive(slot_id)) continue;

			PointComponentSlot& slot = world.data.getPointComponentSlot(slot_id);
			const PointComponentType& type = getPointComponentType(slot.type_id);
			if(slot.uses_external_data && slot.external_data == nullptr)
			{
				danikk_engine::formatFatalError("Point component slot % has no external data", slot_id);
			}
			if(type.tick != nullptr)
			{
				type.tick(slot, world);
			}
		}

		const size_t graph_component_count = graph_component_slot_ids.size();
		for(size_t component_index = 0; component_index < graph_component_count; component_index++)
		{
			const GraphComponentSlotId slot_id = graph_component_slot_ids[component_index];
			if(slot_id >= world.data.getGraphComponentSlotCount())
			{
				danikk_engine::formatFatalError("Chunk references invalid graph component slot %", slot_id);
			}
			if(!world.data.isGraphComponentSlotActive(slot_id)) continue;
			GraphComponentSlot& slot = world.data.getGraphComponentSlot(slot_id);
			const GraphComponentType& type = getGraphComponentType(slot.type_id);
			if(slot.uses_external_data && slot.external_data == nullptr)
			{
				danikk_engine::formatFatalError("Graph component slot % has no external data", slot_id);
			}
			if(type.tick != nullptr) type.tick(slot, world);
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
			if(!world.data.isPointComponentSlotActive(slot_id)) continue;

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

		const size_t graph_component_count = graph_component_slot_ids.size();
		for(size_t component_index = 0; component_index < graph_component_count; component_index++)
		{
			const GraphComponentSlotId slot_id = graph_component_slot_ids[component_index];
			if(slot_id >= world.data.getGraphComponentSlotCount())
			{
				danikk_engine::formatFatalError("Chunk references invalid graph component slot %", slot_id);
			}
			if(!world.data.isGraphComponentSlotActive(slot_id)) continue;
			GraphComponentSlot& slot = world.data.getGraphComponentSlot(slot_id);
			const GraphComponentType& type = getGraphComponentType(slot.type_id);
			if(slot.uses_external_data && slot.external_data == nullptr)
			{
				danikk_engine::formatFatalError("Graph component slot % has no external data", slot_id);
			}
			if(type.frame != nullptr) type.frame(slot, world);
		}
	}



	bool Chunk::hasSharedFace(VolumeCellId cell_id, uint8 face_index) const
	{
		if(!world.data.isVolumeCellActive(cell_id))
		{
			return false;
		}
		const VolumeCell& cell = world.data.getVolumeCell(cell_id);
		for(VolumeCellId other_cell_id : volume_cell_ids)
		{
			if(other_cell_id == cell_id || !world.data.isVolumeCellActive(other_cell_id))
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

	mat4 Chunk::createWorldMatrix(const World& world) const
	{
		const glm::dvec3 relative_position = world.getChunkWorldPosition(chunk_id) - world.getCameraWorldPosition();
		return glm::translate(mat4(1.0f), vec3(relative_position));
	}

	void Chunk::draw(World& world)
	{
		if(mesh == nullptr || material_ranges.size() == 0) return;
		setWorldMatrix(createWorldMatrix(world));
		for(const ChunkMaterialRange& range : material_ranges)
		{
			if(range.index_count == 0) continue;
			MaterialDefinition& material = getMaterial(range.material_id);
			if(material.texture.isNull())
			{
				white_texture.bind();
			}
			else
			{
				material.texture.bind();
			}
			setDrawColor(vec4(material.color, 1.0f));
			mesh->drawRange(range.first_index, range.index_count);
		}
	}
}
