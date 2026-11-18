#pragma once

#include <danikk_space_engine/world/point.h>
#include <danikk_space_engine/component/point/point.h>
#include <danikk_space_engine/world/chunk.h>
#include <danikk_space_engine/world/body.h>
#include <danikk_space_engine/world/resource_pool.h>
#include <danikk_space_engine/component/world_body/world_body.h>
#include <danikk_space_engine/factions.h>

#include <danikk_framework/dynamic_array.h>

namespace danikk_space_engine
{

	/**
	 * @brief Owns indexed pools for points, chunks, world bodies, and component slots.
	 *
	 * Pool IDs are array indices. Owner objects store IDs rather than copies, and their reverse
	 * links let destruction detach resources without searching every owner. An ID is reusable
	 * after destruction, so callers must not keep IDs to resources they no longer own or validate.
	 * `clear()` retains pool capacity for reuse; `release()` also returns that capacity.
	 */
	class WorldData
	{
		/** @brief Stable-ID pool for world-local point positions. */
		WorldResourcePool<Point, PointId> points{"Point"};
		/** @brief Stable-ID pool for chunk geometry and component lists. */
		WorldResourcePool<Chunk, ChunkId> chunks{"Chunk"};
		/** @brief Stable-ID pool for positioned bodies and their owned chunk/component IDs. */
		WorldResourcePool<WorldBody, WorldBodyId> world_bodies{"WorldBody"};
		/** @brief Stable-ID pool for components attached to world bodies. */
		WorldResourcePool<WorldBodyComponentSlot, WorldBodyComponentSlotId> world_body_components{"WorldBodyComponentSlot"};
		/** @brief Stable-ID pool for components attached to points. */
		WorldResourcePool<PointComponentSlot, PointComponentSlotId> point_components{"PointComponentSlot"};
		/** @brief Monotonically advanced token assigned to WorldBody component slot incarnations. */
		uint64 next_world_body_component_generation_id = 1;
		/** @brief Monotonic incarnation token for point-component slots. */
		uint64 next_point_component_generation_id = 1;

	public:
		/** @brief Factions and their symmetric relations in this world. */
		FactionData factions;

		/**
		 * @brief Allocates or reuses a point slot and returns its ID.
		 * @return ID of a default-initialized point; its position is set by the caller.
		 */
		PointId createPoint();

		/**
		 * @brief Allocates or reuses a WorldBody slot and returns its ID.
		 * @return ID of a default-initialized body.
		 */
		WorldBodyId createWorldBody();

		/**
		 * @brief Allocates a WorldBody component slot, constructs its payload, and attaches it to the body.
		 * @param world_body_id Active body that owns the component.
		 * @param type_id Registered component type controlling payload storage and callbacks.
		 * @param init_data Optional type-specific initialization payload; null requests defaults.
		 * @return ID of the constructed component slot.
		 */
		WorldBodyComponentSlotId createWorldBodyComponentSlot(
			WorldBodyId world_body_id,
			WorldBodyComponentTypeId type_id,
			const void* init_data = nullptr);

		/**
		 * @brief Allocates a chunk, sets its owning body ID, and registers it with that body.
		 * @param world_body_id Active body that owns the new chunk.
		 * @return Mutable reference to the initialized chunk resource.
		 */
		Chunk& createChunk(WorldBodyId world_body_id);

		/**
		 * @brief Adds a point ID to a chunk and increments its chunk-reference count once.
		 *
		 * Re-adding a point already listed by the chunk is a no-op. The count tracks
		 * distinct referencing chunks, not components. A point cannot be
		 * referenced by more than 100 chunks. Chunks share points by ID; the point is
		 * destroyed when its count reaches zero during chunk destruction.
		 * @param chunk_id Active chunk that will reference the point.
		 * @param point_id Active point being shared or owned by the chunk.
		 */
		void addPointReferenceToChunk(ChunkId chunk_id, PointId point_id);

		/**
		 * @brief Attaches a point-component slot to the chunk that schedules its tick/frame callbacks.
		 * @param chunk_id Active owning chunk.
		 * @param slot_id Active point-component slot whose point belongs to this chunk.
		 */
		void addPointComponentSlotToChunk(ChunkId chunk_id, PointComponentSlotId slot_id);

		/**
		 * @brief Detaches a point component from its owner and clears the slot's chunk ID.
		 * @param slot_id Active slot to detach. An unattached slot is left alone.
		 * The caller manages the chunk's point reference separately.
		 */
		void removePointComponentSlotFromChunk(PointComponentSlotId slot_id);

		/**
		 * @brief Allocates a point-component slot and constructs its inline or external payload.
		 * @param point_id Active point to which the component is attached.
		 * @param type_id Registered type that provides payload layout and lifecycle callbacks.
		 * @param init_data Optional type-specific initialization data.
		 * @return ID of the constructed slot; add it to its owning chunk to schedule callbacks.
		 */
		PointComponentSlotId createPointComponentSlot(
			PointId point_id,
			PointComponentTypeId type_id,
			const void* init_data = nullptr);

		/**
		 * @brief Destroys every component/edge referencing a point, detaches chunk links, then frees its slot.
		 * @param id Point pool ID; inactive or invalid IDs are ignored.
		 */
		void destroyPoint(PointId id);
		/**
		 * @brief Destroys a chunk's components, triangles, mesh, and point references before freeing its slot.
		 *
		 * A referenced point is freed only after its chunk-reference count reaches zero.
		 * @param id Chunk pool ID; inactive or invalid IDs are ignored.
		 */
		void destroyChunk(ChunkId id);

		/**
		 * @brief Destroys a body's components and all its chunks, then frees the body slot.
		 * @param id WorldBody pool ID; inactive or invalid IDs are ignored.
		 */
		void destroyWorldBody(WorldBodyId id);

		/**
		 * @brief Detaches the component, runs its registered destructor, frees external payload, and recycles the slot.
		 * @param id WorldBody component slot ID; inactive or invalid IDs are ignored.
		 */
		void destroyWorldBodyComponentSlot(WorldBodyComponentSlotId id);

		/**
		 * @brief Detaches the point component from chunks, destroys its payload, and recycles the slot.
		 * @param id Point component slot ID; inactive or invalid IDs are ignored.
		 */
		void destroyPointComponentSlot(PointComponentSlotId id);

		/**
		 * @brief Checks whether the point ID currently refers to an allocated point.
		 * @param id Point pool ID to check.
		 * @return True only when the ID currently identifies an allocated point.
		 */
		bool isPointActive(PointId id) const;

		/**
		 * @brief Checks whether the chunk ID currently refers to an allocated chunk.
		 * @param id Chunk pool ID to check.
		 * @return True only when the ID currently identifies an allocated chunk.
		 */
		bool isChunkActive(ChunkId id) const;

		/**
		 * @brief Checks whether the world-body ID currently refers to an allocated body.
		 * @param id World-body pool ID to check.
		 * @return True only when the ID currently identifies an allocated body.
		 */
		bool isWorldBodyActive(WorldBodyId id) const;

		/**
		 * @brief Checks whether the ID refers to an allocated world-body component slot.
		 * @param id World-body component slot ID to check.
		 * @return True only when the slot is currently allocated.
		 */
		bool isWorldBodyComponentSlotActive(WorldBodyComponentSlotId id) const;

		/**
		 * @brief Checks whether the ID refers to an allocated point component slot.
		 * @param id Point component slot ID to check.
		 * @return True only when the slot is currently allocated.
		 */
		bool isPointComponentSlotActive(PointComponentSlotId id) const;

		/**
		 * @brief Returns the pool-owned list of active point IDs; do not retain it across pool mutations.
		 * @return Const reference to the pool's compact active-ID array.
		 */
		const DynamicArray<PointId>& getUsedPointIds() const;


		/**
		 * @brief Returns the pool-owned list of active chunk IDs; do not retain it across pool mutations.
		 * @return Const reference to the pool's compact active-ID array.
		 */
		const DynamicArray<ChunkId>& getUsedChunkIds() const;

		/**
		 * @brief Returns the pool-owned list of active world-body IDs; do not retain it across pool mutations.
		 * @return Const reference to the pool's compact active-ID array.
		 */
		const DynamicArray<WorldBodyId>& getUsedWorldBodyIds() const;

		/**
		 * @brief Returns the pool-owned list of active world-body component slot IDs.
		 * @return A pool-owned array reference; do not retain it across pool mutations.
		 */
		const DynamicArray<WorldBodyComponentSlotId>& getUsedWorldBodyComponentSlotIds() const;

		/**
		 * @brief Returns the pool-owned list of active point component slot IDs.
		 * @return A pool-owned array reference; do not retain it across pool mutations.
		 */
		const DynamicArray<PointComponentSlotId>& getUsedPointComponentSlotIds() const;

		/**
		 * @brief Returns a checked reference to an active point; an invalid ID raises a fatal error.
		 * @param id Active point pool ID.
		 * @return Mutable reference to the point stored in the pool.
		 */
		Point& getPoint(PointId id);

		/**
		 * @brief Returns a checked reference to an active point; an invalid ID raises a fatal error.
		 * @param id Active point pool ID.
		 * @return Read-only reference to the point stored in the pool.
		 */
		const Point& getPoint(PointId id) const;



		/**
		 * @brief Returns a checked reference to an active chunk; an invalid ID raises a fatal error.
		 * @param id Active chunk pool ID.
		 * @return Mutable reference to the chunk stored in the pool.
		 */
		Chunk& getChunk(ChunkId id);

		/**
		 * @brief Returns a checked reference to an active chunk; an invalid ID raises a fatal error.
		 * @param id Active chunk pool ID.
		 * @return Read-only reference to the chunk stored in the pool.
		 */
		const Chunk& getChunk(ChunkId id) const;

		/**
		 * @brief Returns a checked reference to an active world body; an invalid ID raises a fatal error.
		 * @param id Active world-body pool ID.
		 * @return Mutable reference to the body stored in the pool.
		 */
		WorldBody& getWorldBody(WorldBodyId id);

		/**
		 * @brief Returns a checked reference to an active world body; an invalid ID raises a fatal error.
		 * @param id Active world-body pool ID.
		 * @return Read-only reference to the body stored in the pool.
		 */
		const WorldBody& getWorldBody(WorldBodyId id) const;

		/**
		 * @brief Returns a checked reference to an active world-body component slot.
		 * @param id Active world-body component slot ID.
		 * @return Mutable reference to the slot stored in the pool.
		 */
		WorldBodyComponentSlot& getWorldBodyComponentSlot(WorldBodyComponentSlotId id);

		/**
		 * @brief Returns a checked reference to an active world-body component slot.
		 * @param id Active world-body component slot ID.
		 * @return Read-only reference to the slot stored in the pool.
		 */
		const WorldBodyComponentSlot& getWorldBodyComponentSlot(WorldBodyComponentSlotId id) const;

		/**
		 * @brief Returns a checked reference to an active point component slot.
		 * @param id Active point component slot ID.
		 * @return Mutable reference to the slot stored in the pool.
		 */
		PointComponentSlot& getPointComponentSlot(PointComponentSlotId id);

		/**
		 * @brief Returns a checked reference to an active point component slot.
		 * @param id Active point component slot ID.
		 * @return Read-only reference to the slot stored in the pool.
		 */
		const PointComponentSlot& getPointComponentSlot(PointComponentSlotId id) const;

		/**
		 * @brief Returns the total point-pool slot count, including slots available for reuse.
		 * @return Number of allocated pool slots, not the number of active points.
		 */
		size_t getPointSlotCount() const;


		/**
		 * @brief Returns the total chunk-pool slot count, including reusable slots.
		 * @return Number of allocated pool slots, not the number of active chunks.
		 */
		size_t getChunkSlotCount() const;

		/**
		 * @brief Returns the total world-body pool slot count, including reusable slots.
		 * @return Number of allocated pool slots, not the number of active bodies.
		 */
		size_t getWorldBodySlotCount() const;

		/**
		 * @brief Returns the total world-body component pool slot count.
		 * @return Number of allocated slots, including slots available for reuse.
		 */
		size_t getWorldBodyComponentSlotCount() const;

		/**
		 * @brief Returns the total point-component pool slot count.
		 * @return Number of allocated slots, including slots available for reuse.
		 */
		size_t getPointComponentSlotCount() const;

		/**
		 * @brief Destroys all live resources and empties pool bookkeeping while retaining allocated capacity.
		 *
		 * Every previously active resource ID becomes invalid. Pool arrays retain
		 * capacity so later allocations can reuse their storage.
		 */
		void clear();

		/**
		 * @brief Calls clear() and releases the backing capacity of every WorldData pool.
		 * All resource IDs become invalid, and pool storage is returned to the allocator.
		 */
		void release();
	};
}
