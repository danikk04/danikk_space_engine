#pragma once

#include <danikk_space_engine/default.h>
#include <danikk_space_engine/id.h>
#include <danikk_space_engine/resource_pool.h>
#include <danikk_space_engine/component/point/point.h>

namespace danikk_space_engine
{

	/**
	 * @brief Stores a chunk-local mesh or placement position and its chunk reference count.
	 *
	 * A point is stored once in PointData and can be shared by multiple chunks.
	 * Its reference count measures distinct chunk `point_ids` lists. It does not
	 * count how many point components inside one chunk use it.
	 */
	struct Point
	{
		/** @brief Position relative to the referencing chunk origin, stored in single precision. */
		vec3 position = vec3(0.0f);
		/**
		 * @brief Number of distinct chunks whose `point_ids` lists reference this point (maximum 100).
		 * PointData tracks this once per chunk and frees the point when the
		 * final referencing chunk is destroyed.
		 */
		uint8 referencing_chunk_count = 0;
	};

	/** @brief Owns points and their attached point-component slots. */
	class PointData
	{
		/** @brief Stable-ID pool for point positions. */
		WorldResourcePool<Point, PointId> points{"Point"};
		/** @brief Stable-ID pool for point-component slots. */
		WorldResourcePool<PointComponentSlot, PointComponentSlotId> point_components{"PointComponentSlot"};
		/** @brief Monotonic incarnation token assigned to point-component slots. */
		uint64 next_point_component_generation_id = 1;

	public:
		/** @brief Creates a default point and returns its ID. */
		PointId createPoint();
		/** @brief Creates and initializes a component slot attached to a point. */
		PointComponentSlotId createPointComponentSlot(PointId point_id, PointComponentTypeId type_id,
			const void* init_data = nullptr);
		/** @brief Destroys a point after its chunk references have been detached. */
		void destroyPoint(PointId id);
		/** @brief Destroys a detached point-component slot and its payload. */
		void destroyPointComponentSlot(PointComponentSlotId id);
		/** @brief Checks whether a point ID is active. */
		bool isPointActive(PointId id) const;
		/** @brief Checks whether a point-component slot ID is active. */
		bool isPointComponentSlotActive(PointComponentSlotId id) const;
		/** @brief Returns active point IDs; invalidated by pool mutations. */
		const DynamicArray<PointId>& getUsedPointIds() const;
		/** @brief Returns active point-component IDs; invalidated by pool mutations. */
		const DynamicArray<PointComponentSlotId>& getUsedPointComponentSlotIds() const;
		/** @brief Gets a checked active point. */
		Point& getPoint(PointId id);
		/** @brief Gets a checked active point without mutation access. */
		const Point& getPoint(PointId id) const;
		/** @brief Gets a checked active point-component slot. */
		PointComponentSlot& getPointComponentSlot(PointComponentSlotId id);
		/** @brief Gets a checked active point-component slot without mutation access. */
		const PointComponentSlot& getPointComponentSlot(PointComponentSlotId id) const;
		/** @brief Returns allocated point slots, including reusable slots. */
		size_t getPointSlotCount() const;
		/** @brief Returns allocated point-component slots, including reusable slots. */
		size_t getPointComponentSlotCount() const;
		/** @brief Destroys active point resources while retaining capacity. */
		void clear();
		/** @brief Destroys active point resources and releases capacity. */
		void release();
	};
}
