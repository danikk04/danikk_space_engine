#include <danikk_space_engine/point.h>

namespace danikk_space_engine
{
	namespace
	{
		/** @brief Checks a point-pool ID and returns the active resource. */
		template<typename resource_t, typename id_t>
		resource_t& getCheckedResource(WorldResourcePool<resource_t, id_t>& pool, id_t id, const char* resource_name)
		{
			if(!pool.isActive(id)) danikk_engine::formatFatalError("Invalid % ID %", resource_name, id);
			return pool[id];
		}

		/** @brief Checks a point-pool ID and returns the active resource as const. */
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
	/** @copydoc PointData::isPointComponentSlotActive */
	bool PointData::isPointComponentSlotActive(PointComponentSlotId id) const { return point_components.isActive(id); }
	/** @copydoc PointData::getUsedPointIds */
	const DynamicArray<PointId>& PointData::getUsedPointIds() const { return points.usedIds(); }
	/** @copydoc PointData::getUsedPointComponentSlotIds */
	const DynamicArray<PointComponentSlotId>& PointData::getUsedPointComponentSlotIds() const { return point_components.usedIds(); }
	/** @copydoc PointData::getPoint */
	Point& PointData::getPoint(PointId id) { return getCheckedResource(points, id, "point"); }
	/** @copydoc PointData::getPoint */
	const Point& PointData::getPoint(PointId id) const { return getCheckedResource(points, id, "point"); }
	/** @copydoc PointData::getPointComponentSlot */
	PointComponentSlot& PointData::getPointComponentSlot(PointComponentSlotId id) { return getCheckedResource(point_components, id, "point component slot"); }
	/** @copydoc PointData::getPointComponentSlot */
	const PointComponentSlot& PointData::getPointComponentSlot(PointComponentSlotId id) const { return getCheckedResource(point_components, id, "point component slot"); }
	/** @copydoc PointData::getPointSlotCount */
	size_t PointData::getPointSlotCount() const { return points.size(); }
	/** @copydoc PointData::getPointComponentSlotCount */
	size_t PointData::getPointComponentSlotCount() const { return point_components.size(); }
}
