#include <danikk_space_engine/world/data.h>

namespace danikk_space_engine
{
	namespace
	{
		template<typename component_type_t, typename type_id_t>
		void validateComponentType(const component_type_t& type, type_id_t type_id, const char* component_name)
		{
			if(static_cast<uint32>(type_id) == 0 || type.construct == nullptr || type.data_size == 0)
			{
				danikk_engine::formatFatalError("% component type % cannot be constructed", component_name, static_cast<uint32>(type_id));
			}
			if(type.data_alignment > alignof(std::max_align_t))
			{
				danikk_engine::formatFatalError("% component type % requires unsupported alignment", component_name, static_cast<uint32>(type_id));
			}
		}

		template<typename slot_t, typename component_type_t, typename type_id_t>
		void constructComponentPayload(
			slot_t& slot,
			const component_type_t& type,
			const void* init_data,
			type_id_t type_id,
			const char* component_name)
		{
			slot.uses_external_data = !type.inline_relocatable
				|| type.data_size > slot_t::inline_data_capacity;
			if(slot.uses_external_data)
			{
				slot.external_data = (danikk_framework::malloc)(type.data_size);
				if(slot.external_data == nullptr)
				{
					danikk_engine::formatFatalError(
						"Cannot allocate % bytes for % component type %",
						type.data_size, component_name, static_cast<uint32>(type_id));
				}
			}
			type.construct(slot.getData(), init_data);
		}
	}

	PointId WorldData::createPoint() { return points.create(); }
	VolumeCellId WorldData::createVolumeCell() { return volume_cells.create(); }
	GroundTriangleId WorldData::createGroundTriangle() { return ground_triangles.create(); }
	WorldBodyId WorldData::createWorldBody() { return world_bodies.create(); }

	Chunk& WorldData::createChunk(WorldBodyId world_body_id)
	{
		if(!isWorldBodyActive(world_body_id))
		{
			danikk_engine::formatFatalError("Chunk references invalid world body %", world_body_id);
		}
		const ChunkId id = chunks.create();
		Chunk& chunk = chunks[id];
		chunk.chunk_id = id;
		chunk.world_body_id = world_body_id;
		chunk.grid_coordinate = glm::ivec3(0);
		getWorldBody(world_body_id).addChunk(id);
		return chunk;
	}

	PointComponentSlotId WorldData::createPointComponentSlot(
		PointId point_id,
		PointComponentTypeId type_id,
		const void* init_data)
	{
		if(!isPointActive(point_id))
		{
			danikk_engine::formatFatalError("Point component references invalid point %", point_id);
		}

		const PointComponentType& type = getPointComponentType(type_id);
		validateComponentType(type, type_id, "Point");

		const PointComponentSlotId id = point_components.create();
		PointComponentSlot& slot = point_components[id];
		slot.point_id = point_id;
		slot.chunk_id = static_cast<uint32>(-1);
		slot.type_id = type_id;
		constructComponentPayload(slot, type, init_data, type_id, "point");
		return id;
	}

	// Creates the graph slot payload after validating its chunk, endpoint points,
	// type registration, and alignment. It then adds references to both endpoints
	// and registers the finished slot with the owning chunk.
	GraphComponentSlotId WorldData::createGraphComponentSlot(
		ChunkId chunk_id,
		PointId point_a,
		PointId point_b,
		GraphComponentTypeId type_id,
		const void* init_data)
	{
		if(!isChunkActive(chunk_id))
		{
			danikk_engine::formatFatalError("Graph component references invalid chunk %", chunk_id);
		}
		if(!isPointActive(point_a) || !isPointActive(point_b))
		{
			danikk_engine::formatFatalError("Graph component references invalid endpoint points % and %", point_a, point_b);
		}

		const GraphComponentType& type = getGraphComponentType(type_id);
		validateComponentType(type, type_id, "Graph");

		const GraphComponentSlotId id = graph_components.create();
		GraphComponentSlot& slot = graph_components[id];
		slot.point_a = point_a;
		slot.point_b = point_b;
		slot.type_id = type_id;
		constructComponentPayload(slot, type, init_data, type_id, "graph");
		Chunk& chunk = getChunk(chunk_id);
		addPointReferenceToChunk(chunk_id, point_a);
		addPointReferenceToChunk(chunk_id, point_b);
		chunk.addGraphComponentSlot(id);
		return id;
	}
}
