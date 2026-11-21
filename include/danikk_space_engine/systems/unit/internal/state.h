#pragma once

#include <danikk_framework/dynamic_array.h>
#include <danikk_space_engine/systems/unit/unit.h>
#include <danikk_space_engine/component/point/human/human.h>

namespace danikk_space_engine::unit_system_internal
{
	/** @brief Pooled point-component identity used for safe enemy references. */
	struct UnitTarget
	{
		PointComponentSlotId slot_id = static_cast<PointComponentSlotId>(-1);
		PointId point_id = static_cast<PointId>(-1);
		uint64 generation_id = 0;
	};

	/** @brief One live human's identity and body-local position in the sparse unit index. */
	struct UnitSpatialEntry
	{
		UnitTarget target;
		glm::dvec3 position = glm::dvec3(0.0);
	};

	/** @brief Sparse octree cell containing unit entry indices until its population exceeds capacity. */
	struct UnitSpatialNode
	{
		static constexpr uint32 invalid_child = static_cast<uint32>(-1);
		glm::dvec3 center = glm::dvec3(0.0);
		double half_extent = 0.0;
		uint32 children[8] = {invalid_child, invalid_child, invalid_child, invalid_child,
			invalid_child, invalid_child, invalid_child, invalid_child};
		DynamicArray<uint32> entry_indices;

		bool hasChildren() const
		{
			for(uint32 child_id : children)
				if(child_id != invalid_child) return true;
			return false;
		}
	};

	/** @brief Per-WorldBody sparse unit octree; cells split by occupancy, not a target dimension. */
	struct UnitSpatialTree
	{
		WorldBodyId body_id = static_cast<WorldBodyId>(-1);
		bool active = false;
		DynamicArray<UnitSpatialEntry> entries;
		DynamicArray<UnitSpatialNode> nodes;
		uint32 root_id = UnitSpatialNode::invalid_child;
		uint32 node_count = 0;
	};

	/** @brief Rebuilt broadphase index used to find nearby human agents without scanning chunks. */
	struct UnitSpatialIndex
	{
		DynamicArray<UnitSpatialTree> trees;
	};

	/** @brief Current action values assigned by the unit system to one human. */
	struct UnitOrder
	{
		bool has_move_target = false;
		glm::dvec3 move_target_world = glm::dvec3(0.0);
		bool has_look_target = false;
		glm::dvec3 look_target_world = glm::dvec3(0.0);
		bool has_attack_target = false;
		PointComponentSlotId target_slot_id = static_cast<PointComponentSlotId>(-1);
		PointId target_point_id = static_cast<PointId>(-1);
		uint64 target_generation_id = 0;
		bool has_equip_order = false;
		ItemDefinitionId equip_item_definition_id = static_cast<ItemDefinitionId>(-1);
		bool should_fire = false;
		ItemDefinitionId fire_item_definition_id = static_cast<ItemDefinitionId>(-1);

		/** @brief Clears actions and invalidates any current target. */
		void clear()
		{
			has_move_target = false;
			has_look_target = false;
			has_attack_target = false;
			target_slot_id = static_cast<PointComponentSlotId>(-1);
			target_point_id = static_cast<PointId>(-1);
			target_generation_id = 0;
			has_equip_order = false;
			equip_item_definition_id = static_cast<ItemDefinitionId>(-1);
			should_fire = false;
			fire_item_definition_id = static_cast<ItemDefinitionId>(-1);
		}
	};

	/** @brief Central AI state and current order for one registered human. */
	struct UnitAgentState
	{
		PointComponentSlotId human_slot_id = static_cast<PointComponentSlotId>(-1);
		uint64 human_generation_id = 0;
		bool enabled = true;
		UnitGlobalState global_state = UnitGlobalState::rest;
		UnitSubstate substate = UnitSubstate::idle;
		DynamicArray<UnitTarget> enemy_candidates;
		size_t candidate_index = 0;
		UnitOrder order;
	};

	/** @brief One faction's report that a hostile unit was seen. */
	struct UnitSighting
	{
		FactionId faction_id = static_cast<FactionId>(-1);
		PointComponentSlotId target_slot_id = static_cast<PointComponentSlotId>(-1);
		PointId target_point_id = static_cast<PointId>(-1);
		uint64 target_generation_id = 0;
		glm::dvec3 position = glm::dvec3(0.0);
		WorldBodyId body_id = static_cast<WorldBodyId>(-1);
		glm::dvec3 body_local_position = glm::dvec3(0.0);
		uint32 last_seen_timestamp_ms = 0;
		uint32 last_attack_timestamp_ms = 0;
		PointId last_attacked_point_id = static_cast<PointId>(-1);
		bool visible_last_tick = false;
		bool visible_this_tick = false;
		bool visibility_checked_this_tick = false;
	};

	/** @brief A remembered hostile contact location used by battle reconnaissance. */
	struct UnitSearchLocation
	{
		WorldBodyId body_id = static_cast<WorldBodyId>(-1);
		glm::dvec3 body_local_position = glm::dvec3(0.0);
		PointComponentSlotId target_slot_id = static_cast<PointComponentSlotId>(-1);
		uint64 target_generation_id = 0;
		PointComponentSlotId assigned_unit_slot_id = static_cast<PointComponentSlotId>(-1);
		bool is_corpse = false;
	};

	/** @brief Kind of remembered location assigned by battle or work behavior. */
	enum class UnitSearchTaskType : uint8
	{
		lost_contact,
		corpse_recovery
	};

	/** @brief Tick-local candidate data reused while assigning one faction's search tasks. */
	struct UnitSearchCandidate
	{
		PointComponentSlotId slot_id = static_cast<PointComponentSlotId>(-1);
		PointComponentSlot* slot = nullptr;
		UnitAgentState* agent = nullptr;
		HumanComponent* human = nullptr;
		WorldBodyId body_id = static_cast<WorldBodyId>(-1);
		glm::dvec3 position = glm::dvec3(0.0);
		bool assigned_to_search = false;
	};

	/** @brief Persistent dispersal position and scan phase for one battle recon unit. */
	struct UnitPostCombatAssignment
	{
		PointComponentSlotId unit_slot_id = static_cast<PointComponentSlotId>(-1);
		uint64 unit_generation_id = 0;
		WorldBodyId body_id = static_cast<WorldBodyId>(-1);
		glm::dvec3 body_local_position = glm::dvec3(0.0);
		float scan_angle = 0.0f;
		uint32 next_scan_timestamp_ms = 0;
	};

	/** @brief Per-faction tactical memory, agents, and behavior scratch storage. */
	struct UnitGroup
	{
		FactionId faction_id = static_cast<FactionId>(-1);
		DynamicArray<UnitSighting> known_sightings;
		DynamicArray<UnitAgentState> agents;
		DynamicArray<UnitSearchLocation> search_locations;
		/** @brief Reusable temporary storage with pointers valid only during task assignment. */
		DynamicArray<UnitSearchCandidate> search_candidates;
		/** @brief Stable dispersal destinations and scan phases during battle reconnaissance. */
		DynamicArray<UnitPostCombatAssignment> post_combat_assignments;
		/** @brief Group-wide time since any faction unit last saw or reported a hostile. */
		float time_since_enemy_visible = 0.0f;
		bool enemy_seen_this_tick = false;
		bool attack_reported_this_tick = false;
		bool has_had_enemy_contact = false;
		bool war_active = false;
	};
}

namespace danikk_space_engine
{
	/** @brief Private storage owned by one World::unit_system instance. */
	struct UnitSystem::Impl
	{
		DynamicArray<unit_system_internal::UnitGroup> groups;
		unit_system_internal::UnitSpatialIndex spatial_index;
	};
}
