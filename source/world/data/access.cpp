#include <danikk_space_engine/world/data.h>

namespace danikk_space_engine
{
	namespace
	{

		template<typename resource_t, typename id_t>
		/**
		 * @brief Returns checked resource.
		 */
		resource_t& getCheckedResource(WorldResourcePool<resource_t, id_t>& pool, id_t id, const char* resource_name)
		{
			if(!pool.isActive(id)) danikk_engine::formatFatalError("Invalid % ID %", resource_name, id);
			return pool[id];
		}

		/**
		 * @brief Returns checked resource.
		 */
		template<typename resource_t, typename id_t>
		/**
		 * @brief Returns checked resource.
		 */
		const resource_t& getCheckedResource(
			const WorldResourcePool<resource_t, id_t>& pool,
			id_t id,
			const char* resource_name)
		{
			if(!pool.isActive(id)) danikk_engine::formatFatalError("Invalid % ID %", resource_name, id);
			return pool[id];
		}
	}

	bool WorldData::isPointActive(PointId id) const { return points.isActive(id); }
	bool WorldData::isGroundTriangleActive(GroundTriangleId id) const { return ground_triangles.isActive(id); }
	bool WorldData::isChunkActive(ChunkId id) const { return chunks.isActive(id); }
	bool WorldData::isWorldBodyActive(WorldBodyId id) const { return world_bodies.isActive(id); }
	bool WorldData::isWorldBodyComponentSlotActive(WorldBodyComponentSlotId id) const { return world_body_components.isActive(id); }
	bool WorldData::isPointComponentSlotActive(PointComponentSlotId id) const { return point_components.isActive(id); }
	bool WorldData::isGraphComponentSlotActive(GraphComponentSlotId id) const { return graph_components.isActive(id); }

	const DynamicArray<PointId>& WorldData::getUsedPointIds() const { return points.usedIds(); }
	const DynamicArray<GroundTriangleId>& WorldData::getUsedGroundTriangleIds() const { return ground_triangles.usedIds(); }
	const DynamicArray<ChunkId>& WorldData::getUsedChunkIds() const { return chunks.usedIds(); }
	const DynamicArray<WorldBodyId>& WorldData::getUsedWorldBodyIds() const { return world_bodies.usedIds(); }
	const DynamicArray<WorldBodyComponentSlotId>& WorldData::getUsedWorldBodyComponentSlotIds() const { return world_body_components.usedIds(); }
	const DynamicArray<PointComponentSlotId>& WorldData::getUsedPointComponentSlotIds() const { return point_components.usedIds(); }
	const DynamicArray<GraphComponentSlotId>& WorldData::getUsedGraphComponentSlotIds() const { return graph_components.usedIds(); }

	Point& WorldData::getPoint(PointId id) { return getCheckedResource(points, id, "point"); }
	const Point& WorldData::getPoint(PointId id) const { return getCheckedResource(points, id, "point"); }
	GroundTriangle& WorldData::getGroundTriangle(GroundTriangleId id) { return getCheckedResource(ground_triangles, id, "ground triangle"); }
	const GroundTriangle& WorldData::getGroundTriangle(GroundTriangleId id) const { return getCheckedResource(ground_triangles, id, "ground triangle"); }
	Chunk& WorldData::getChunk(ChunkId id) { return getCheckedResource(chunks, id, "chunk"); }
	const Chunk& WorldData::getChunk(ChunkId id) const { return getCheckedResource(chunks, id, "chunk"); }
	WorldBody& WorldData::getWorldBody(WorldBodyId id) { return getCheckedResource(world_bodies, id, "world body"); }
	const WorldBody& WorldData::getWorldBody(WorldBodyId id) const { return getCheckedResource(world_bodies, id, "world body"); }
	WorldBodyComponentSlot& WorldData::getWorldBodyComponentSlot(WorldBodyComponentSlotId id) { return getCheckedResource(world_body_components, id, "world body component slot"); }
	const WorldBodyComponentSlot& WorldData::getWorldBodyComponentSlot(WorldBodyComponentSlotId id) const { return getCheckedResource(world_body_components, id, "world body component slot"); }
	PointComponentSlot& WorldData::getPointComponentSlot(PointComponentSlotId id) { return getCheckedResource(point_components, id, "point component slot"); }
	const PointComponentSlot& WorldData::getPointComponentSlot(PointComponentSlotId id) const { return getCheckedResource(point_components, id, "point component slot"); }
	GraphComponentSlot& WorldData::getGraphComponentSlot(GraphComponentSlotId id) { return getCheckedResource(graph_components, id, "graph component slot"); }
	const GraphComponentSlot& WorldData::getGraphComponentSlot(GraphComponentSlotId id) const { return getCheckedResource(graph_components, id, "graph component slot"); }

	size_t WorldData::getPointSlotCount() const { return points.size(); }
	size_t WorldData::getGroundTriangleSlotCount() const { return ground_triangles.size(); }
	size_t WorldData::getChunkSlotCount() const { return chunks.size(); }
	size_t WorldData::getWorldBodySlotCount() const { return world_bodies.size(); }
	size_t WorldData::getWorldBodyComponentSlotCount() const { return world_body_components.size(); }
	size_t WorldData::getPointComponentSlotCount() const { return point_components.size(); }
	size_t WorldData::getGraphComponentSlotCount() const { return graph_components.size(); }
}
