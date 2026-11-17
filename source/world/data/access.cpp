#include <danikk_space_engine/world/data.h>

namespace danikk_space_engine
{
	namespace
	{

		/**
		 * @brief Returns a mutable pool entry after checking that its ID is active.
		 * @tparam resource_t Pooled resource type.
		 * @tparam id_t Resource ID type.
		 * @param pool Pool that owns the resource.
		 * @param id ID to validate and access.
		 * @param resource_name Name included in the fatal diagnostic on invalid access.
		 * @return Mutable reference to the active resource.
		 */
		template<typename resource_t, typename id_t>
		resource_t& getCheckedResource(WorldResourcePool<resource_t, id_t>& pool, id_t id, const char* resource_name)
		{
			if(!pool.isActive(id)) danikk_engine::formatFatalError("Invalid % ID %", resource_name, id);
			return pool[id];
		}

		/**
		 * @brief Returns a read-only pool entry after checking that its ID is active.
		 * @tparam resource_t Pooled resource type.
		 * @tparam id_t Resource ID type.
		 * @param pool Pool that owns the resource.
		 * @param id ID to validate and access.
		 * @param resource_name Name included in the fatal diagnostic on invalid access.
		 * @return Read-only reference to the active resource.
		 */
		template<typename resource_t, typename id_t>
		const resource_t& getCheckedResource(
			const WorldResourcePool<resource_t, id_t>& pool,
			id_t id,
			const char* resource_name)
		{
			if(!pool.isActive(id)) danikk_engine::formatFatalError("Invalid % ID %", resource_name, id);
			return pool[id];
		}
	}

	/** @copydoc WorldData::isPointActive */
	bool WorldData::isPointActive(PointId id) const { return points.isActive(id); }
	/** @copydoc WorldData::isChunkActive */
	bool WorldData::isChunkActive(ChunkId id) const { return chunks.isActive(id); }
	/** @copydoc WorldData::isWorldBodyActive */
	bool WorldData::isWorldBodyActive(WorldBodyId id) const { return world_bodies.isActive(id); }
	/** @copydoc WorldData::isWorldBodyComponentSlotActive */
	bool WorldData::isWorldBodyComponentSlotActive(WorldBodyComponentSlotId id) const { return world_body_components.isActive(id); }
	/** @copydoc WorldData::isPointComponentSlotActive */
	bool WorldData::isPointComponentSlotActive(PointComponentSlotId id) const { return point_components.isActive(id); }
	/** @copydoc WorldData::isGraphComponentSlotActive */
	bool WorldData::isGraphComponentSlotActive(GraphComponentSlotId id) const { return graph_components.isActive(id); }

	/** @copydoc WorldData::getUsedPointIds */
	const DynamicArray<PointId>& WorldData::getUsedPointIds() const { return points.usedIds(); }
	/** @copydoc WorldData::getUsedChunkIds */
	const DynamicArray<ChunkId>& WorldData::getUsedChunkIds() const { return chunks.usedIds(); }
	/** @copydoc WorldData::getUsedWorldBodyIds */
	const DynamicArray<WorldBodyId>& WorldData::getUsedWorldBodyIds() const { return world_bodies.usedIds(); }
	/** @copydoc WorldData::getUsedWorldBodyComponentSlotIds */
	const DynamicArray<WorldBodyComponentSlotId>& WorldData::getUsedWorldBodyComponentSlotIds() const { return world_body_components.usedIds(); }
	/** @copydoc WorldData::getUsedPointComponentSlotIds */
	const DynamicArray<PointComponentSlotId>& WorldData::getUsedPointComponentSlotIds() const { return point_components.usedIds(); }
	/** @copydoc WorldData::getUsedGraphComponentSlotIds */
	const DynamicArray<GraphComponentSlotId>& WorldData::getUsedGraphComponentSlotIds() const { return graph_components.usedIds(); }

	/** @copydoc WorldData::getPoint */
	Point& WorldData::getPoint(PointId id) { return getCheckedResource(points, id, "point"); }
	/** @copydoc WorldData::getPoint */
	const Point& WorldData::getPoint(PointId id) const { return getCheckedResource(points, id, "point"); }
	/** @copydoc WorldData::getChunk */
	Chunk& WorldData::getChunk(ChunkId id) { return getCheckedResource(chunks, id, "chunk"); }
	/** @copydoc WorldData::getChunk */
	const Chunk& WorldData::getChunk(ChunkId id) const { return getCheckedResource(chunks, id, "chunk"); }
	/** @copydoc WorldData::getWorldBody */
	WorldBody& WorldData::getWorldBody(WorldBodyId id) { return getCheckedResource(world_bodies, id, "world body"); }
	/** @copydoc WorldData::getWorldBody */
	const WorldBody& WorldData::getWorldBody(WorldBodyId id) const { return getCheckedResource(world_bodies, id, "world body"); }
	/** @copydoc WorldData::getWorldBodyComponentSlot */
	WorldBodyComponentSlot& WorldData::getWorldBodyComponentSlot(WorldBodyComponentSlotId id) { return getCheckedResource(world_body_components, id, "world body component slot"); }
	/** @copydoc WorldData::getWorldBodyComponentSlot */
	const WorldBodyComponentSlot& WorldData::getWorldBodyComponentSlot(WorldBodyComponentSlotId id) const { return getCheckedResource(world_body_components, id, "world body component slot"); }
	/** @copydoc WorldData::getPointComponentSlot */
	PointComponentSlot& WorldData::getPointComponentSlot(PointComponentSlotId id) { return getCheckedResource(point_components, id, "point component slot"); }
	/** @copydoc WorldData::getPointComponentSlot */
	const PointComponentSlot& WorldData::getPointComponentSlot(PointComponentSlotId id) const { return getCheckedResource(point_components, id, "point component slot"); }
	/** @copydoc WorldData::getGraphComponentSlot */
	GraphComponentSlot& WorldData::getGraphComponentSlot(GraphComponentSlotId id) { return getCheckedResource(graph_components, id, "graph component slot"); }
	/** @copydoc WorldData::getGraphComponentSlot */
	const GraphComponentSlot& WorldData::getGraphComponentSlot(GraphComponentSlotId id) const { return getCheckedResource(graph_components, id, "graph component slot"); }

	/** @copydoc WorldData::getPointSlotCount */
	size_t WorldData::getPointSlotCount() const { return points.size(); }
	/** @copydoc WorldData::getChunkSlotCount */
	size_t WorldData::getChunkSlotCount() const { return chunks.size(); }
	/** @copydoc WorldData::getWorldBodySlotCount */
	size_t WorldData::getWorldBodySlotCount() const { return world_bodies.size(); }
	/** @copydoc WorldData::getWorldBodyComponentSlotCount */
	size_t WorldData::getWorldBodyComponentSlotCount() const { return world_body_components.size(); }
	/** @copydoc WorldData::getPointComponentSlotCount */
	size_t WorldData::getPointComponentSlotCount() const { return point_components.size(); }
	/** @copydoc WorldData::getGraphComponentSlotCount */
	size_t WorldData::getGraphComponentSlotCount() const { return graph_components.size(); }
}
