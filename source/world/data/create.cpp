#include <danikk_space_engine/world/data.h>

namespace danikk_space_engine
{
	namespace
	{

		/**
		 * @brief Validates type registration and payload alignment before constructing a component.
		 * @tparam component_type_t Component callback descriptor type.
		 * @tparam type_id_t Registered component ID type.
		 * @param type Callback metadata to validate.
		 * @param type_id ID used for diagnostics and to reject the reserved `none` entry.
		 * @param component_name Component category label for fatal diagnostics.
		 */
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

		/**
		 * @brief Chooses payload storage and calls the registered placement constructor.
		 * @tparam slot_t Component slot type providing inline storage and external pointer fields.
		 * @tparam component_type_t Callback descriptor type.
		 * @tparam type_id_t Component ID type.
		 * @param slot Slot receiving the payload.
		 * @param type Descriptor providing size, relocation policy, and construct callback.
		 * @param init_data Optional initialization data forwarded to the callback.
		 * @param type_id ID used for allocation diagnostics.
		 * @param component_name Component category label for fatal diagnostics.
		 */
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

	/** @copydoc WorldData::createPoint */
	PointId WorldData::createPoint() { return points.create(); }
	/** @copydoc WorldData::createGroundTriangle */
	GroundTriangleId WorldData::createGroundTriangle() { return ground_triangles.create(); }
	/** @copydoc WorldData::createWorldBody */
	WorldBodyId WorldData::createWorldBody() { return world_bodies.create(); }

	/** @copydoc WorldData::createWorldBodyComponentSlot */
	WorldBodyComponentSlotId WorldData::createWorldBodyComponentSlot(
		WorldBodyId world_body_id,
		WorldBodyComponentTypeId type_id,
		const void* init_data)
	{

		if(!isWorldBodyActive(world_body_id))
		{
			danikk_engine::formatFatalError("World body component references invalid body %", world_body_id);
		}
		const WorldBodyComponentType& type = getWorldBodyComponentType(type_id);
		validateComponentType(type, type_id, "World body");

		const WorldBodyComponentSlotId id = world_body_components.create();
		WorldBodyComponentSlot& slot = world_body_components[id];
		slot.slot_id = id;
		slot.world_body_id = world_body_id;
		slot.generation_id = next_world_body_component_generation_id++;
		slot.type_id = type_id;
		constructComponentPayload(slot, type, init_data, type_id, "world body");
		getWorldBody(world_body_id).addComponentSlot(id);
		return id;
	}

	/**
	 * @brief Creates a chunk owned by a body and registers the reverse body-to-chunk link.
	 *
	 * New chunks start with an origin grid coordinate and zero local position; generation or
	 * placement code assigns their spatial coordinates afterward.
	 * @param world_body_id Active body that will own the chunk.
	 * @return Mutable reference to the initialized chunk resource.
	 */
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

	/** @copydoc WorldData::createPointComponentSlot */
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

	/**
	 * @brief Creates an edge payload and registers it with its owning chunk and endpoints.
	 *
	 * Validates the chunk and both endpoint IDs, constructs the payload using the registered type,
	 * adds each endpoint to the chunk's distinct point-reference list, then appends the graph slot
	 * to the chunk's graph-component list.
	 * @param chunk_id Active chunk that owns and schedules the graph component.
	 * @param point_a First active endpoint.
	 * @param point_b Second active endpoint.
	 * @param type_id Registered graph-component type.
	 * @param init_data Optional type-specific initialization data.
	 * @return ID of the constructed graph-component slot.
	 */
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
