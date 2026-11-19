#include <danikk_space_engine/point.h>
#include <danikk_space_engine/world_body.h>

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

	/** @copydoc PointData::isPointActive */
	bool PointData::isPointActive(PointId id) const { return points.isActive(id); }
	/** @copydoc WorldBodyData::isWorldBodyActive */
	bool WorldBodyData::isWorldBodyActive(WorldBodyId id) const { return world_bodies.isActive(id); }
	/** @copydoc WorldBodyData::isWorldBodyComponentSlotActive */
	bool WorldBodyData::isWorldBodyComponentSlotActive(WorldBodyComponentSlotId id) const { return world_body_components.isActive(id); }
	/** @copydoc PointData::isPointComponentSlotActive */
	bool PointData::isPointComponentSlotActive(PointComponentSlotId id) const { return point_components.isActive(id); }

	/** @copydoc PointData::getUsedPointIds */
	const DynamicArray<PointId>& PointData::getUsedPointIds() const { return points.usedIds(); }
	/** @copydoc WorldBodyData::getUsedWorldBodyIds */
	const DynamicArray<WorldBodyId>& WorldBodyData::getUsedWorldBodyIds() const { return world_bodies.usedIds(); }
	/** @copydoc WorldBodyData::getUsedWorldBodyComponentSlotIds */
	const DynamicArray<WorldBodyComponentSlotId>& WorldBodyData::getUsedWorldBodyComponentSlotIds() const { return world_body_components.usedIds(); }
	/** @copydoc PointData::getUsedPointComponentSlotIds */
	const DynamicArray<PointComponentSlotId>& PointData::getUsedPointComponentSlotIds() const { return point_components.usedIds(); }

	/** @copydoc PointData::getPoint */
	Point& PointData::getPoint(PointId id) { return getCheckedResource(points, id, "point"); }
	/** @copydoc PointData::getPoint */
	const Point& PointData::getPoint(PointId id) const { return getCheckedResource(points, id, "point"); }
	/** @copydoc WorldBodyData::getWorldBody */
	WorldBody& WorldBodyData::getWorldBody(WorldBodyId id) { return getCheckedResource(world_bodies, id, "world body"); }
	/** @copydoc WorldBodyData::getWorldBody */
	const WorldBody& WorldBodyData::getWorldBody(WorldBodyId id) const { return getCheckedResource(world_bodies, id, "world body"); }
	/** @copydoc WorldBodyData::getWorldBodyComponentSlot */
	WorldBodyComponentSlot& WorldBodyData::getWorldBodyComponentSlot(WorldBodyComponentSlotId id) { return getCheckedResource(world_body_components, id, "world body component slot"); }
	/** @copydoc WorldBodyData::getWorldBodyComponentSlot */
	const WorldBodyComponentSlot& WorldBodyData::getWorldBodyComponentSlot(WorldBodyComponentSlotId id) const { return getCheckedResource(world_body_components, id, "world body component slot"); }
	/** @copydoc PointData::getPointComponentSlot */
	PointComponentSlot& PointData::getPointComponentSlot(PointComponentSlotId id) { return getCheckedResource(point_components, id, "point component slot"); }
	/** @copydoc PointData::getPointComponentSlot */
	const PointComponentSlot& PointData::getPointComponentSlot(PointComponentSlotId id) const { return getCheckedResource(point_components, id, "point component slot"); }

	/** @copydoc PointData::getPointSlotCount */
	size_t PointData::getPointSlotCount() const { return points.size(); }
	/** @copydoc WorldBodyData::getWorldBodySlotCount */
	size_t WorldBodyData::getWorldBodySlotCount() const { return world_bodies.size(); }
	/** @copydoc WorldBodyData::getWorldBodyComponentSlotCount */
	size_t WorldBodyData::getWorldBodyComponentSlotCount() const { return world_body_components.size(); }
	/** @copydoc PointData::getPointComponentSlotCount */
	size_t PointData::getPointComponentSlotCount() const { return point_components.size(); }
}
