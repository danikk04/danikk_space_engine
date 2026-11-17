#pragma once

#include <danikk_framework/dynamic_array.h>

#include <danikk_space_engine/world/chunk.h>
#include <danikk_space_engine/component/world_body/world_body.h>
#include <danikk_space_engine/factions.h>

namespace danikk_space_engine
{

	/**
	 * @brief Represents a positioned world body and indexes its chunks and components.
	 *
	 * `chunk_ids` and `component_slot_ids` contain pooled resource IDs managed by
	 * WorldData; destroying the body releases those resources. The lists avoid
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
		/** @brief Linear velocity in world units per second, integrated by WorldPhysics. */
		glm::dvec3 linear_velocity = glm::dvec3(0.0);
		/** @brief Angular velocity vector in radians per second, integrated by WorldPhysics. */
		glm::dvec3 angular_velocity = glm::dvec3(0.0);
		/** @brief Body mass used by gravity and collision response. */
		double mass = 1.0;
		/** @brief Conservative radius used by body-level broadphase collision checks. */
		double collision_radius = 0.0;
		/**
		 * @brief IDs of chunks assigned to this body's local coordinate system.
		 * The IDs are membership/index data; chunk objects live in WorldData.
		 */
		DynamicArray<ChunkId> chunk_ids;
		/**
		 * @brief IDs of component slots attached to this body.
		 * The slots and payloads live in WorldData's component pool.
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
}
