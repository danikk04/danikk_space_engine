#pragma once

#include <danikk_space_engine/id.h>
#include <danikk_space_engine/fwd.h>
#include <danikk_framework/glm.h>
#include <danikk_framework/dynamic_array.h>

namespace danikk_space_engine
{
	/** @brief Strategic state shared by the registered units of one faction. */
	enum class UnitGlobalState : uint8
	{
		war,
		work,
		rest
	};

	/** @brief Current task within a unit's strategic state. */
	enum class UnitSubstate : uint8
	{
		pursuit,
		attack,
		reconnaissance,
		moving_to_work,
		collecting_corpses,
		sleep,
		idle
	};

	/**
	 * @brief Owns faction-scoped unit groups, their tactical memory, and assigned orders.
	 *
	 * The implementation data is private to the unit-system module and lives for the
	 * lifetime of the owning World.
	 */
	class UnitSystem
	{
	public:
		/** @brief Opaque storage for per-faction unit-group data. */
		struct Impl;

	private:
		/** @brief World whose faction and unit data this system manages. */
		World& world;
		/** @brief Owned implementation containing all group and tactical state. */
		Impl* impl = nullptr;

	public:
		/** @brief Resets unit groups and logs subsystem initialization. */
		void init();
		/** @brief Resets unit groups and logs subsystem shutdown. */
		void shutdown();
		explicit UnitSystem(World& world);
		~UnitSystem();
		UnitSystem(const UnitSystem&) = delete;
		UnitSystem& operator=(const UnitSystem&) = delete;

		/** @brief Runs perception, chooses each faction group's mode, then dispatches its behavior. */
		void tick(float dt);
		/** @brief Records hostile fire as fresh tactical information for the victim's faction. */
		void reportAttack(HumanId attacker_id, HumanId victim_id);
		/** @brief Adds or updates a human in the group associated with its faction. */
		void registerAgent(HumanId human_id, bool enabled = true);
		/** @brief Gets a live unit's assigned movement target. */
		bool getMoveTarget(HumanId human_id, uint64 generation_id,
			glm::dvec3& target_world) const;
		/** @brief Gets a registered unit's current hierarchical tactical state. */
		bool getState(HumanId human_id, uint64 generation_id,
			UnitGlobalState& global_state, UnitSubstate& substate) const;
		/** @brief Collects human slots in a body-local box using the current adaptive spatial tree. */
		void collectNearbyHumanIds(WorldBodyId body_id, const glm::dvec3& center,
			double half_extent, DynamicArray<HumanId>& result) const;
		/** @brief Copies indexed human component IDs and their owning bodies for spatial systems. */
		void collectHumanIds(DynamicArray<HumanId>& human_ids,
			DynamicArray<WorldBodyId>& body_ids) const;
		/** @brief Clears all faction groups when the owning world resets or shuts down. */
		void reset();
	};

}
