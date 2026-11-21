#include <algorithm>
#include <cmath>
#include <limits>

#include <danikk_space_engine/profiler.h>
#include <danikk_space_engine/world.h>
#include <danikk_space_engine/systems/unit/internal/perception.h>
#include <danikk_space_engine/systems/unit/internal/state.h>

namespace danikk_space_engine::unit_system_internal
{
	namespace
	{
		constexpr size_t unit_spatial_leaf_capacity = 8;
		constexpr uint8 unit_spatial_max_depth = 24;
		constexpr double unit_spatial_query_half_extent = 64.0;

		uint8 getOctant(const glm::dvec3& position, const glm::dvec3& center)
		{
			return static_cast<uint8>((position.x >= center.x ? 1 : 0)
				| (position.y >= center.y ? 2 : 0)
				| (position.z >= center.z ? 4 : 0));
		}

		UnitSpatialTree* findOrCreateTree(UnitSpatialIndex& index, WorldBodyId body_id)
		{
			for(UnitSpatialTree& tree : index.trees)
				if(tree.body_id == body_id)
				{
					tree.active = true;
					return &tree;
				}
			UnitSpatialTree& tree = index.trees.pushCtor();
			tree.body_id = body_id;
			tree.active = true;
			return &tree;
		}

		uint32 allocateNode(UnitSpatialTree& tree)
		{
			const uint32 node_id = tree.node_count++;
			if(node_id >= tree.nodes.size()) tree.nodes.pushCtor();
			UnitSpatialNode& node = tree.nodes[node_id];
			node.center = glm::dvec3(0.0);
			node.half_extent = 0.0;
			for(uint32& child_id : node.children) child_id = UnitSpatialNode::invalid_child;
			node.entry_indices.clear();
			return node_id;
		}

		void insertEntry(UnitSpatialTree& tree, uint32 node_id, uint32 entry_id, uint8 depth)
		{
			const bool leaf = !tree.nodes[node_id].hasChildren();
			if(leaf && (tree.nodes[node_id].entry_indices.size() < unit_spatial_leaf_capacity
				|| depth >= unit_spatial_max_depth))
			{
				tree.nodes[node_id].entry_indices.push(entry_id);
				return;
			}

			if(leaf)
			{
				const glm::dvec3 parent_center = tree.nodes[node_id].center;
				const double child_half_extent = tree.nodes[node_id].half_extent * 0.5;
				uint32 child_ids[8];
				for(uint8 octant = 0; octant < 8; ++octant)
				{
					const uint32 child_id = allocateNode(tree);
					UnitSpatialNode& child = tree.nodes[child_id];
					child.center = parent_center + glm::dvec3(
						(octant & 1) ? child_half_extent : -child_half_extent,
						(octant & 2) ? child_half_extent : -child_half_extent,
						(octant & 4) ? child_half_extent : -child_half_extent);
					child.half_extent = child_half_extent;
					child_ids[octant] = child_id;
				}
				for(uint8 octant = 0; octant < 8; ++octant)
					tree.nodes[node_id].children[octant] = child_ids[octant];

				uint32 previous_entries[unit_spatial_leaf_capacity];
				const size_t previous_count = tree.nodes[node_id].entry_indices.size();
				for(size_t i = 0; i < previous_count; ++i)
					previous_entries[i] = tree.nodes[node_id].entry_indices[i];
				tree.nodes[node_id].entry_indices.clear();
				for(size_t i = 0; i < previous_count; ++i)
				{
					const uint32 old_entry = previous_entries[i];
					const uint8 octant = getOctant(tree.entries[old_entry].position, parent_center);
					insertEntry(tree, child_ids[octant], old_entry, static_cast<uint8>(depth + 1));
				}
			}

			const glm::dvec3 node_center = tree.nodes[node_id].center;
			const uint8 octant = getOctant(tree.entries[entry_id].position, node_center);
			const uint32 child_id = tree.nodes[node_id].children[octant];
			insertEntry(tree, child_id, entry_id, static_cast<uint8>(depth + 1));
		}

		void buildTree(UnitSpatialTree& tree)
		{
			tree.node_count = 0;
			if(tree.entries.size() == 0)
			{
				tree.root_id = UnitSpatialNode::invalid_child;
				return;
			}
			glm::dvec3 minimum = tree.entries[0].position;
			glm::dvec3 maximum = minimum;
			for(const UnitSpatialEntry& entry : tree.entries)
			{
				minimum = glm::min(minimum, entry.position);
				maximum = glm::max(maximum, entry.position);
			}
			const glm::dvec3 center = (minimum + maximum) * 0.5;
			const glm::dvec3 half_range = (maximum - minimum) * 0.5;
			double half_extent = std::max({half_range.x, half_range.y, half_range.z, 1.0});
			double power_of_two_extent = 1.0;
			while(power_of_two_extent < half_extent) power_of_two_extent *= 2.0;

			const uint32 root_id = allocateNode(tree);
			UnitSpatialNode& root = tree.nodes[root_id];
			root.center = center;
			root.half_extent = power_of_two_extent;
			tree.root_id = root_id;
			for(uint32 entry_id = 0; entry_id < tree.entries.size(); ++entry_id)
				insertEntry(tree, tree.root_id, entry_id, 0);
		}

		bool intersectsQuery(const UnitSpatialNode& node, const glm::dvec3& center, double half_extent)
		{
			for(uint8 axis = 0; axis < 3; ++axis)
				if(std::abs(node.center[axis] - center[axis]) > node.half_extent + half_extent)
					return false;
			return true;
		}

		bool containsQueryPoint(const glm::dvec3& position, const glm::dvec3& center, double half_extent)
		{
			return std::abs(position.x - center.x) <= half_extent
				&& std::abs(position.y - center.y) <= half_extent
				&& std::abs(position.z - center.z) <= half_extent;
		}

		void collectNearby(const UnitSpatialTree& tree, uint32 node_id,
			const glm::dvec3& center, FactionId faction, World& world, UnitAgentState& agent)
		{
			if(node_id >= tree.node_count) return;
			const UnitSpatialNode& node = tree.nodes[node_id];
			if(!intersectsQuery(node, center, unit_spatial_query_half_extent)) return;
			if(node.hasChildren())
			{
				for(uint32 child_id : node.children)
					if(child_id != UnitSpatialNode::invalid_child)
						collectNearby(tree, child_id, center, faction, world, agent);
				return;
			}

			for(uint32 entry_id : node.entry_indices)
			{
				if(entry_id >= tree.entries.size()) continue;
				const UnitSpatialEntry& entry = tree.entries[entry_id];
				if(!containsQueryPoint(entry.position, center, unit_spatial_query_half_extent)) continue;
				if(isValidEnemy(world, entry.target, faction)) agent.enemy_candidates.push(entry.target);
			}
		}

		void collectSlotsInBox(const UnitSpatialTree& tree, uint32 node_id,
			const glm::dvec3& center, double half_extent, DynamicArray<PointComponentSlotId>& result)
		{
			if(node_id >= tree.node_count) return;
			const UnitSpatialNode& node = tree.nodes[node_id];
			for(uint8 axis = 0; axis < 3; ++axis)
				if(std::abs(node.center[axis] - center[axis]) > node.half_extent + half_extent) return;
			if(node.hasChildren())
			{
				for(uint32 child_id : node.children)
					if(child_id != UnitSpatialNode::invalid_child)
						collectSlotsInBox(tree, child_id, center, half_extent, result);
				return;
			}
			for(uint32 entry_id : node.entry_indices)
			{
				if(entry_id >= tree.entries.size()) continue;
				const UnitSpatialEntry& entry = tree.entries[entry_id];
				if(std::abs(entry.position.x - center.x) > half_extent
					|| std::abs(entry.position.y - center.y) > half_extent
					|| std::abs(entry.position.z - center.z) > half_extent) continue;
				result.push(entry.target.slot_id);
			}
		}
	}

	/** @brief Rebuilds body-local sparse unit octrees, splitting cells only when occupancy exceeds capacity. */
	void rebuildUnitSpatialIndex(World& world, UnitSpatialIndex& index)
	{
		DSE_WATCH();
		for(UnitSpatialTree& tree : index.trees)
		{
			tree.active = false;
			tree.entries.clear();
			tree.node_count = 0;
			tree.root_id = UnitSpatialNode::invalid_child;
			for(UnitSpatialNode& node : tree.nodes) node.entry_indices.clear();
		}
		for(PointComponentSlotId slot_id : world.point_data.getUsedPointComponentSlotIds())
		{
			if(!world.point_data.isPointComponentSlotActive(slot_id)) continue;
			const PointComponentSlot& slot = world.point_data.getPointComponentSlot(slot_id);
			if(slot.type_id != PointComponentTypeId::human
				|| !world.point_data.isPointActive(slot.point_id)
				|| !world.chunks.isChunkActive(slot.chunk_id)) continue;
			const Chunk& chunk = world.chunks.getChunk(slot.chunk_id);
			const glm::dvec3 position = chunk.position
				+ glm::dvec3(world.point_data.getPoint(slot.point_id).position);
			UnitSpatialTree* tree = findOrCreateTree(index, chunk.world_body_id);
			UnitSpatialEntry& entry = tree->entries.pushCtor();
			entry.target = UnitTarget{slot_id, slot.point_id, slot.generation_id};
			entry.position = position;
		}
		for(UnitSpatialTree& tree : index.trees)
			if(tree.active) buildTree(tree);
	}

	/** @brief Collects nearby hostile humans by pruning nonintersecting octree cells. */
	void collectEnemyTargets(World& world, ChunkId chunk_id, FactionId faction,
		const UnitSpatialIndex& index, UnitAgentState& agent)
	{
		DSE_WATCH();
		agent.enemy_candidates.clear();
		agent.candidate_index = 0;
		if(!world.chunks.isChunkActive(chunk_id)
			|| !world.point_data.isPointComponentSlotActive(agent.human_slot_id)) return;
		const Chunk& owner = world.chunks.getChunk(chunk_id);
		const PointComponentSlot& agent_slot = world.point_data.getPointComponentSlot(agent.human_slot_id);
		if(agent_slot.generation_id != agent.human_generation_id
			|| !world.point_data.isPointActive(agent_slot.point_id)) return;
		const glm::dvec3 position = owner.position
			+ glm::dvec3(world.point_data.getPoint(agent_slot.point_id).position);
		for(const UnitSpatialTree& tree : index.trees)
		{
			if(!tree.active || tree.body_id != owner.world_body_id
				|| tree.root_id == UnitSpatialNode::invalid_child) continue;
			collectNearby(tree, tree.root_id, position, faction, world, agent);
			return;
		}
	}

	/** @brief Collects unit slots in a body-local axis-aligned query box. */
	void collectNearbyHumanSlots(const UnitSpatialIndex& index, WorldBodyId body_id,
		const glm::dvec3& center, double half_extent, DynamicArray<PointComponentSlotId>& result)
	{
		DSE_WATCH();
		result.clear();
		if(half_extent < 0.0) return;
		for(const UnitSpatialTree& tree : index.trees)
		{
			if(!tree.active || tree.body_id != body_id || tree.root_id == UnitSpatialNode::invalid_child) continue;
			collectSlotsInBox(tree, tree.root_id, center, half_extent, result);
			return;
		}
	}

	/** @brief Copies the current octree's human slot and owning-body pairs. */
	void collectHumanSlots(const UnitSpatialIndex& index,
		DynamicArray<PointComponentSlotId>& slot_ids, DynamicArray<WorldBodyId>& body_ids)
	{
		DSE_WATCH();
		slot_ids.clear();
		body_ids.clear();
		for(const UnitSpatialTree& tree : index.trees)
		{
			if(!tree.active) continue;
			for(const UnitSpatialEntry& entry : tree.entries)
			{
				slot_ids.push(entry.target.slot_id);
				body_ids.push(tree.body_id);
			}
		}
	}
}
