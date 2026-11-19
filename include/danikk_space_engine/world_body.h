#pragma once

#include <danikk_framework/dynamic_array.h>

#include <danikk_space_engine/systems/chunk/chunk.h>
#include <danikk_space_engine/resource_pool.h>
#include <danikk_space_engine/component/world_body/world_body.h>
#include <danikk_space_engine/systems/faction/faction.h>

namespace danikk_space_engine
{

	/**
	 * @brief Represents a positioned world body and indexes its chunks and components.
	 *
	 * `chunk_ids` and `component_slot_ids` contain pooled resource IDs managed by
	 * WorldBodyData; destroying the body releases its component slots. Body
	 * destruction is coordinated by WorldBodyData with ChunkSystem and PlanetSystem. The lists avoid
	 * duplicate IDs and preserve insertion order. Position, orientation, and
	 * velocity are expressed in world coordinates, while the body's chunks use
	 * body-local coordinates.
	 */
	class WorldBody
	{
	public:
		/** @brief Faction owning this body, or no_faction if unassigned. */
		FactionId faction_id = no_faction;
		/** @brief Body origin in double-precision world coordinates. */
		glm::dvec3 position = glm::dvec3(0.0);
		/** @brief Body orientation mapping body-local coordinates into world orientation. */
		glm::dquat rotation = glm::dquat(1.0, 0.0, 0.0, 0.0);
		/** @brief Linear velocity in world units per second, integrated by PhysicsSystem. */
		glm::dvec3 linear_velocity = glm::dvec3(0.0);
		/** @brief Angular velocity vector in radians per second, integrated by PhysicsSystem. */
		glm::dvec3 angular_velocity = glm::dvec3(0.0);
		/** @brief Body mass used by gravity and collision response. */
		double mass = 1.0;
		/** @brief Conservative radius used by body-level broadphase collision checks. */
		double collision_radius = 0.0;
		/**
		 * @brief IDs of chunks assigned to this body's local coordinate system.
		 * The IDs are membership/index data; chunk objects live in ChunkSystem.
		 */
		DynamicArray<ChunkId> chunk_ids;
		/**
		 * @brief IDs of component slots attached to this body.
		 * The slots and payloads live in WorldBodyData's component pool.
		 */
		DynamicArray<WorldBodyComponentSlotId> component_slot_ids;

		/**
		 * @brief Adds a chunk ID to this body's membership list if absent.
		 * @param chunk_id Chunk owned by this body.
		 * @note Performs a linear duplicate check; the first insertion order is retained.
		 */
		void addChunk(ChunkId chunk_id);

		/**
		 * @brief Removes the first matching chunk ID while preserving the other entries' order.
		 * @param chunk_id Chunk ID to detach.
		 * @return True if the ID was present.
		 */
		bool removeChunk(ChunkId chunk_id);

		/**
		 * @brief Adds a component slot ID to this body's membership list if absent.
		 * @param slot_id Component slot attached to this body.
		 * @note Performs a linear duplicate check; the first insertion order is retained.
		 */
		void addComponentSlot(WorldBodyComponentSlotId slot_id);

		/**
		 * @brief Removes the first matching component slot ID while preserving list order.
		 * @param slot_id Component slot ID to detach.
		 * @return True if the ID was present.
		 */
		bool removeComponentSlot(WorldBodyComponentSlotId slot_id);
	};

	/** @brief Owns WorldBody instances and their attached world-body component slots. */
	class WorldBodyData
	{
		/** @brief Stable-ID pool for positioned WorldBody resources. */
		WorldResourcePool<WorldBody, WorldBodyId> world_bodies{"WorldBody"};
		/** @brief Stable-ID pool for world-body component slots. */
		WorldResourcePool<WorldBodyComponentSlot, WorldBodyComponentSlotId> world_body_components{"WorldBodyComponentSlot"};
		/** @brief Monotonic incarnation token assigned to body-component slots. */
		uint64 next_world_body_component_generation_id = 1;

	public:
		/** @brief Creates a default WorldBody and returns its ID. */
		WorldBodyId createWorldBody();
		/** @brief Creates a component slot attached to an active WorldBody. */
		WorldBodyComponentSlotId createWorldBodyComponentSlot(WorldBodyId world_body_id,
			WorldBodyComponentTypeId type_id, const void* init_data = nullptr);
		/** @brief Destroys a body's planet record, chunks, component slots, and body resource. */
		void destroyWorldBody(WorldBodyId id, ChunkSystem& chunks, PlanetSystem& planets);
		/** @brief Destroys a body-component slot and its payload. */
		void destroyWorldBodyComponentSlot(WorldBodyComponentSlotId id);
		/** @brief Checks whether a WorldBody ID is active. */
		bool isWorldBodyActive(WorldBodyId id) const;
		/** @brief Checks whether a WorldBody component slot ID is active. */
		bool isWorldBodyComponentSlotActive(WorldBodyComponentSlotId id) const;
		/** @brief Returns active WorldBody IDs; invalidated by pool mutations. */
		const DynamicArray<WorldBodyId>& getUsedWorldBodyIds() const;
		/** @brief Returns active WorldBody component IDs; invalidated by pool mutations. */
		const DynamicArray<WorldBodyComponentSlotId>& getUsedWorldBodyComponentSlotIds() const;
		/** @brief Gets a checked active WorldBody. */
		WorldBody& getWorldBody(WorldBodyId id);
		/** @brief Gets a checked active WorldBody without mutation access. */
		const WorldBody& getWorldBody(WorldBodyId id) const;
		/** @brief Gets a checked active body-component slot. */
		WorldBodyComponentSlot& getWorldBodyComponentSlot(WorldBodyComponentSlotId id);
		/** @brief Gets a checked active body-component slot without mutation access. */
		const WorldBodyComponentSlot& getWorldBodyComponentSlot(WorldBodyComponentSlotId id) const;
		/** @brief Returns allocated WorldBody slots, including reusable slots. */
		size_t getWorldBodySlotCount() const;
		/** @brief Returns allocated body-component slots, including reusable slots. */
		size_t getWorldBodyComponentSlotCount() const;
		/** @brief Destroys active body resources while retaining capacity. */
		void clear();
		/** @brief Destroys active body resources and releases capacity. */
		void release();
	};
}
