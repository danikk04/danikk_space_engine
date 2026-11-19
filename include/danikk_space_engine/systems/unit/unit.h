#pragma once

#include <danikk_space_engine/id.h>
#include <danikk_space_engine/fwd.h>
#include <danikk_framework/glm.h>

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
		explicit UnitSystem(World& world);
		~UnitSystem();
		UnitSystem(const UnitSystem&) = delete;
		UnitSystem& operator=(const UnitSystem&) = delete;

		/** @brief Updates each faction's perception, tactical state, and unit orders. */
		void tick(float dt);
		/** @brief Records hostile fire as fresh tactical information for the victim's faction. */
		void reportAttack(PointId attacker_point_id, PointId victim_point_id);
		/** @brief Adds or updates a human in the group associated with its faction. */
		void registerAgent(PointComponentSlotId human_slot_id, bool enabled = true);
		/** @brief Gets a live unit's assigned movement target. */
		bool getMoveTarget(PointComponentSlotId human_slot_id, uint64 generation_id,
			glm::dvec3& target_world) const;
		/** @brief Gets a registered unit's current hierarchical tactical state. */
		bool getState(PointComponentSlotId human_slot_id, uint64 generation_id,
			UnitGlobalState& global_state, UnitSubstate& substate) const;
		/** @brief Clears all faction groups when the owning world resets or shuts down. */
		void reset();
	};

}
