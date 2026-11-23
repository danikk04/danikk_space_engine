#include <danikk_space_engine/systems/world_body/system.h>

#include <cmath>
#include <limits>

namespace danikk_space_engine
{
	namespace
	{
		constexpr uint32 invalid_node_id = ~uint32(0);

		uint32 allocateNode(WorldBodySpatialTree& tree, WorldBodySpatialNode&& node)
		{
			if(tree.free_node_ids.size() > 0)
			{
				const uint32 id = tree.free_node_ids[tree.free_node_ids.size() - 1];
				tree.free_node_ids.resize(tree.free_node_ids.size() - 1);
				tree.nodes[id] = static_cast<WorldBodySpatialNode&&>(node);
				return id;
			}
			const uint32 id = static_cast<uint32>(tree.nodes.size());
			tree.nodes.pushMove(node);
			return id;
		}

		uint8 getOctant(const glm::dvec3& position, const glm::dvec3& center)
		{
			return static_cast<uint8>((position.x >= center.x ? 1 : 0)
				| (position.y >= center.y ? 2 : 0) | (position.z >= center.z ? 4 : 0));
		}

		void ensureTreeRoot(WorldBodySpatialTree& tree, const glm::dvec3& position)
		{
			if(tree.root_id == invalid_node_id)
			{
				WorldBodySpatialNode root;
				root.half_extent = WorldBodySpatialTree::root_half_extent;
				tree.root_id = allocateNode(tree, static_cast<WorldBodySpatialNode&&>(root));
			}
			while(true)
			{
				const WorldBodySpatialNode& root = tree.nodes[tree.root_id];
				if(glm::all(glm::lessThanEqual(glm::abs(position - root.center),
					glm::dvec3(root.half_extent)))) return;
				const glm::dvec3 old_center = root.center;
				const double old_half_extent = root.half_extent;
				WorldBodySpatialNode expanded;
				expanded.center = old_center + glm::dvec3(
					position.x >= old_center.x ? old_half_extent : -old_half_extent,
					position.y >= old_center.y ? old_half_extent : -old_half_extent,
					position.z >= old_center.z ? old_half_extent : -old_half_extent);
				expanded.half_extent = old_half_extent * 2.0;
				const uint8 old_octant = getOctant(old_center, expanded.center);
				const uint32 old_root_id = tree.root_id;
				const uint32 expanded_id = allocateNode(tree, static_cast<WorldBodySpatialNode&&>(expanded));
				tree.nodes[expanded_id].children[old_octant] = old_root_id;
				tree.root_id = expanded_id;
			}
		}

		void insertTreeEntry(WorldBodySpatialTree& tree, uint32 node_id, const WorldBodySpatialEntry& entry)
		{
			if(tree.nodes[node_id].hasChildren())
			{
				const uint8 octant = getOctant(entry.position, tree.nodes[node_id].center);
				uint32 child_id = tree.nodes[node_id].children[octant];
				if(child_id == invalid_node_id)
				{
					WorldBodySpatialNode child;
					const WorldBodySpatialNode& parent = tree.nodes[node_id];
					child.half_extent = parent.half_extent * 0.5;
					child.center = parent.center + glm::dvec3(
						(octant & 1) ? child.half_extent : -child.half_extent,
						(octant & 2) ? child.half_extent : -child.half_extent,
						(octant & 4) ? child.half_extent : -child.half_extent);
					child_id = allocateNode(tree, static_cast<WorldBodySpatialNode&&>(child));
					tree.nodes[node_id].children[octant] = child_id;
				}
				insertTreeEntry(tree, child_id, entry);
				return;
			}

			tree.nodes[node_id].entries.push(entry);
			if(tree.nodes[node_id].entries.size() <= WorldBodySpatialTree::split_threshold
				|| tree.nodes[node_id].half_extent <= WorldBodySpatialTree::minimum_half_extent) return;

			const glm::dvec3 parent_center = tree.nodes[node_id].center;
			const double child_half_extent = tree.nodes[node_id].half_extent * 0.5;
			for(uint8 octant = 0; octant < 8; ++octant)
			{
				WorldBodySpatialNode child;
				child.half_extent = child_half_extent;
				child.center = parent_center + glm::dvec3(
					(octant & 1) ? child_half_extent : -child_half_extent,
					(octant & 2) ? child_half_extent : -child_half_extent,
					(octant & 4) ? child_half_extent : -child_half_extent);
				tree.nodes[node_id].children[octant] = allocateNode(tree,
					static_cast<WorldBodySpatialNode&&>(child));
			}
			DynamicArray<WorldBodySpatialEntry> entries = static_cast<DynamicArray<WorldBodySpatialEntry>&&>(tree.nodes[node_id].entries);
			tree.nodes[node_id].entries.clear();
			for(const WorldBodySpatialEntry& old_entry : entries)
				insertTreeEntry(tree, tree.nodes[node_id].children[getOctant(old_entry.position, parent_center)], old_entry);
		}

		bool removeTreeEntry(WorldBodySpatialTree& tree, uint32 node_id,
			PointId point_id, const glm::dvec3& position)
		{
			WorldBodySpatialNode& node = tree.nodes[node_id];
			if(node.hasChildren())
			{
				const uint8 octant = getOctant(position, node.center);
				const uint32 child_id = node.children[octant];
				if(child_id == invalid_node_id
					|| !removeTreeEntry(tree, child_id, point_id, position)) return false;
				const WorldBodySpatialNode& child = tree.nodes[child_id];
				if(child.entries.size() == 0 && !child.hasChildren())
				{
					node.children[octant] = invalid_node_id;
					tree.nodes[child_id] = WorldBodySpatialNode();
					tree.free_node_ids.push(child_id);
				}
				return true;
			}
			for(size_t index = 0; index < node.entries.size(); ++index)
			{
				if(node.entries[index].point_id != point_id) continue;
				for(size_t shift = index + 1; shift < node.entries.size(); ++shift) node.entries[shift - 1] = node.entries[shift];
				node.entries.resize(node.entries.size() - 1);
				return true;
			}
			return false;
		}

		bool rayIntersectsBounds(const glm::dvec3& origin, const glm::dvec3& direction,
			double max_distance, const glm::dvec3& center, double half_extent)
		{
			double minimum = 0.0;
			double maximum = max_distance;
			for(uint8 axis = 0; axis < 3; ++axis)
			{
				if(std::abs(direction[axis]) < 1e-12)
				{
					if(origin[axis] < center[axis] - half_extent || origin[axis] > center[axis] + half_extent) return false;
					continue;
				}
				double a = (center[axis] - half_extent - origin[axis]) / direction[axis];
				double b = (center[axis] + half_extent - origin[axis]) / direction[axis];
				if(a > b)
				{
					const double temporary = a;
					a = b;
					b = temporary;
				}
				minimum = glm::max(minimum, a);
				maximum = glm::min(maximum, b);
				if(minimum > maximum) return false;
			}
			return true;
		}

		void collectRayEntries(const WorldBodySpatialTree& tree, uint32 node_id,
			const glm::dvec3& origin, const glm::dvec3& direction, double max_distance,
			DynamicArray<PointId>& output)
		{
			const WorldBodySpatialNode& node = tree.nodes[node_id];
			if(!rayIntersectsBounds(origin, direction, max_distance, node.center, node.half_extent + 4.0)) return;
			for(const WorldBodySpatialEntry& entry : node.entries)
			{
				const glm::dvec3 offset = entry.position - origin;
				const double along = glm::dot(offset, direction);
				if(along < -4.0 || along > max_distance + 4.0) continue;
				const glm::dvec3 closest = origin + direction * glm::clamp(along, 0.0, max_distance);
				if(glm::dot(entry.position - closest, entry.position - closest) <= 16.0)
					output.push(entry.point_id);
			}
		for(uint32 child_id : node.children)
			if(child_id != invalid_node_id) collectRayEntries(tree, child_id, origin, direction, max_distance, output);
		}

		void collectBoxEntries(const WorldBodySpatialTree& tree, uint32 node_id,
			const glm::dvec3& center, double half_extent, DynamicArray<PointId>& output)
		{
			const WorldBodySpatialNode& node = tree.nodes[node_id];
			for(uint8 axis = 0; axis < 3; ++axis)
				if(std::abs(node.center[axis] - center[axis]) > node.half_extent + half_extent) return;
			if(node.hasChildren())
			{
				for(uint32 child_id : node.children)
					if(child_id != invalid_node_id) collectBoxEntries(tree, child_id, center, half_extent, output);
				return;
			}
			for(const WorldBodySpatialEntry& entry : node.entries)
				if(glm::all(glm::lessThanEqual(glm::abs(entry.position - center), glm::dvec3(half_extent))))
					output.push(entry.point_id);
		}
	}

	void WorldBodySpatialTree::insert(PointId point_id, const glm::dvec3& position)
	{
		ensureTreeRoot(*this, position);
		insertTreeEntry(*this, root_id, WorldBodySpatialEntry{point_id, position});
	}

	void WorldBodySpatialTree::remove(PointId point_id, const glm::dvec3& position)
	{
		if(root_id == invalid_node_id || !removeTreeEntry(*this, root_id, point_id, position)) return;
		const WorldBodySpatialNode& root = nodes[root_id];
		if(root.entries.size() == 0 && !root.hasChildren())
		{
			nodes.clear();
			free_node_ids.clear();
			root_id = invalid_node_id;
		}
	}

	void WorldBodySpatialTree::collectRayCandidates(const glm::dvec3& origin,
		const glm::dvec3& direction, double max_distance, DynamicArray<PointId>& output) const
	{
		if(root_id != invalid_node_id) collectRayEntries(*this, root_id, origin, direction, max_distance, output);
	}

	void WorldBodySpatialTree::collectBoxCandidates(const glm::dvec3& center,
		double half_extent, DynamicArray<PointId>& output) const
	{
		if(root_id != invalid_node_id && half_extent >= 0.0)
			collectBoxEntries(*this, root_id, center, half_extent, output);
	}

}
