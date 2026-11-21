#pragma once

#include <array>
#include <algorithm>
#include <cmath>
#include <memory>
#include <limits>

#include <danikk_framework/array.h>
#include <danikk_framework/dynamic_array.h>
#include <danikk_space_engine/fwd.h>
#include <danikk_space_engine/id.h>
#include <danikk_space_engine/mesh_render_data.h>
#include <danikk_space_engine/systems/planet/edit.h>
#include <danikk_space_engine/systems/mesh_generation/mesh_generation.h>

namespace danikk_space_engine
{
	namespace planet_settings_ids
	{
		inline constexpr PlanetSettingsId earth = 0;
		inline constexpr PlanetSettingsId count = 1;
	}

	inline constexpr double default_planet_radius = 6371000.0;
	inline constexpr double default_planet_core_radius = 3480000.0;
	inline constexpr double planet_voxel_size = 2.0;

	/** @brief Immutable settings used when registering a planet for a WorldBody. */
	struct PlanetSettings
	{
		const char* name = "Earth";
		double radius = default_planet_radius;
		double core_radius = default_planet_core_radius;
		double surface_gravity = 9.8;
		/** @brief RGB color of the atmosphere near the planet surface. */
		glm::vec3 atmosphere_color = glm::vec3(0.2f, 0.4f, 0.8f);
		/** @brief Height above the surface where the atmosphere fades completely to space. */
		double atmosphere_height = 100000.0;
		uint64 noise_seed = 0;
	};

	/** @brief Controls which generated and edited terrain fields a planet query samples. */
	enum class PlanetTerrainMode : uint8
	{
		heightmap, ///< Procedural surface only; excludes caves and terrain edits.
		heightmap_with_edits, ///< Procedural surface plus terrain edits; excludes caves.
		cave_field, ///< Procedural terrain with caves; excludes terrain edits.
		cave_field_with_edits ///< Full volumetric terrain with caves and terrain edits.
	};

	/** @brief One edit reference held by the octree node containing its full bounds. */
	struct PlanetTerrainEditReference
	{
		PlanetTerrainEditId edit_id = 0;
		uint64 order = 0;
	};

	/** @brief Spatial node of the planet's terrain-edit octree. */
	struct PlanetTerrainEditLocation
	{
		uint32 node_id = static_cast<uint32>(-1);
		size_t entry_index = static_cast<size_t>(-1);
		uint64 order = 0;
	};

	struct PlanetTerrainEditNode
	{
		glm::dvec3 center = glm::dvec3(0.0);
		double half_extent = 0.0;
		uint32 children[8] = {
			static_cast<uint32>(-1), static_cast<uint32>(-1),
			static_cast<uint32>(-1), static_cast<uint32>(-1),
			static_cast<uint32>(-1), static_cast<uint32>(-1),
			static_cast<uint32>(-1), static_cast<uint32>(-1)};
		DynamicArray<PlanetTerrainEditReference> edits;

		bool hasChildren() const
		{
			for(uint32 child_id : children)
				if(child_id != static_cast<uint32>(-1)) return true;
			return false;
		}
	};

	/** @brief Owns all edit records and a spatial octree used to query candidate edits. */
	struct PlanetTerrainEdits
	{
		static constexpr double initial_root_half_extent = 8388608.0;
		static constexpr double minimum_cell_size = 4.0;
		static constexpr size_t split_edit_threshold = 8;

		std::shared_ptr<PlanetTerrainEditPool> pool =
			std::make_shared<PlanetTerrainEditPool>("PlanetTerrainEdit");
		DynamicArray<PlanetTerrainEditId> ids;
		DynamicArray<PlanetTerrainEditNode> nodes;
		DynamicArray<PlanetTerrainEditLocation> locations;
		uint32 root_id = static_cast<uint32>(-1);
		uint64 next_order = 0;

		PlanetTerrainEditId append(const PlanetTerrainEdit& edit);
		bool remove(PlanetTerrainEditId edit_id);

		template<typename callback_t>
		void forEachCandidate(const glm::dvec3& position, callback_t&& callback) const
		{
			if(root_id == static_cast<uint32>(-1) || root_id >= nodes.size()) return;
			std::array<const DynamicArray<PlanetTerrainEditReference>*, 64> sources{};
			size_t source_count = 0;
			uint32 node_id = root_id;
			while(node_id < nodes.size() && source_count < sources.size())
			{
				const PlanetTerrainEditNode& node = nodes[node_id];
				if(node.edits.size() > 0) sources[source_count++] = &node.edits;
				if(!node.hasChildren()) break;
				const uint8 child_index = static_cast<uint8>((position.x >= node.center.x ? 1 : 0)
					| (position.y >= node.center.y ? 2 : 0)
					| (position.z >= node.center.z ? 4 : 0));
				const uint32 child_id = node.children[child_index];
				if(child_id == static_cast<uint32>(-1)) break;
			node_id = child_id;
			}

			std::array<size_t, 64> cursors{};
			while(true)
			{
				size_t best_source = source_count;
				uint64 best_order = std::numeric_limits<uint64>::max();
				for(size_t source = 0; source < source_count; ++source)
				{
					const auto& edits = *sources[source];
					if(cursors[source] >= edits.size()) continue;
					if(edits[cursors[source]].order < best_order)
					{
						best_order = edits[cursors[source]].order;
						best_source = source;
					}
				}
				if(best_source == source_count) break;
				const PlanetTerrainEditId edit_id = sources[best_source]->operator[](cursors[best_source]++).edit_id;
				if(!pool->isActive(edit_id)) continue;
				const PlanetTerrainEdit& edit = (*pool)[edit_id];
				const glm::dvec3 delta = position - edit.getCenter();
				if(glm::dot(delta, delta) <= edit.getRadius() * edit.getRadius()) callback(edit_id);
			}
		}

		/** @brief Visits active edits whose spheres intersect an outward ray from the planet center. */
		template<typename callback_t>
		void forEachRayCandidate(const glm::dvec3& origin, const glm::dvec3& direction,
			double maximum_distance, callback_t&& callback) const
		{
			if(root_id == static_cast<uint32>(-1) || root_id >= nodes.size()) return;
			DynamicArray<uint32> stack;
			DynamicArray<PlanetTerrainEditReference> candidates;
			stack.push(root_id);
			while(stack.size() > 0)
			{
				const uint32 node_id = stack[stack.size() - 1];
				stack.resize(stack.size() - 1);
				if(node_id >= nodes.size()) continue;
				const PlanetTerrainEditNode& node = nodes[node_id];
				const glm::dvec3 minimum = node.center - glm::dvec3(node.half_extent);
				const glm::dvec3 maximum = node.center + glm::dvec3(node.half_extent);
				double near_distance = 0.0;
				double far_distance = maximum_distance;
				bool intersects = true;
				for(uint8 axis = 0; axis < 3; ++axis)
				{
					if(std::abs(direction[axis]) <= 1e-15)
					{
						if(origin[axis] < minimum[axis] || origin[axis] > maximum[axis]) intersects = false;
						continue;
					}
					double first = (minimum[axis] - origin[axis]) / direction[axis];
					double second = (maximum[axis] - origin[axis]) / direction[axis];
					if(first > second)
					{
						const double temporary = first;
						first = second;
						second = temporary;
					}
					near_distance = glm::max(near_distance, first);
					far_distance = glm::min(far_distance, second);
					if(near_distance > far_distance) intersects = false;
				}
				if(!intersects) continue;
				for(const PlanetTerrainEditReference& reference : node.edits) candidates.push(reference);
				for(uint32 child_id : node.children)
					if(child_id != static_cast<uint32>(-1)) stack.push(child_id);
			}
			std::sort(candidates.begin(), candidates.end(), [](const auto& left, const auto& right)
				{ return left.order < right.order; });
			for(const PlanetTerrainEditReference& reference : candidates)
			{
				if(!pool->isActive(reference.edit_id)) continue;
				const PlanetTerrainEdit& edit = (*pool)[reference.edit_id];
				const glm::dvec3 offset = edit.getCenter() - origin;
				const double along = glm::dot(offset, direction);
				if(along + edit.getRadius() < 0.0 || along - edit.getRadius() > maximum_distance) continue;
				const glm::dvec3 closest = origin + direction * glm::clamp(along, 0.0, maximum_distance);
				const glm::dvec3 delta = edit.getCenter() - closest;
				if(glm::dot(delta, delta) <= edit.getRadius() * edit.getRadius()) callback(reference.edit_id);
			}
		}
	};

	/** @brief One adaptive spatial cell in a planet's render/streaming octree. */
	struct PlanetTerrainNode
	{
		glm::dvec3 center = glm::dvec3(0.0);
		double half_extent = 0.0;
		PlanetTerrainNodeId children[8] = {
			static_cast<PlanetTerrainNodeId>(-1), static_cast<PlanetTerrainNodeId>(-1),
			static_cast<PlanetTerrainNodeId>(-1), static_cast<PlanetTerrainNodeId>(-1),
			static_cast<PlanetTerrainNodeId>(-1), static_cast<PlanetTerrainNodeId>(-1),
			static_cast<PlanetTerrainNodeId>(-1), static_cast<PlanetTerrainNodeId>(-1)};
		MeshRenderData mesh;
		glm::dvec3 mesh_position = glm::dvec3(0.0);
		uint64 generation_id = 1;
		uint64 mesh_revision = 0;
		PlanetMeshGenerator mesh_generator = PlanetMeshGenerator::marching_tetrahedra;
		bool mesh_surface_only = true;
		bool generation_pending = false;
		bool render_active = false;

		bool hasChildren() const
		{
			for(PlanetTerrainNodeId child_id : children)
				if(child_id != static_cast<PlanetTerrainNodeId>(-1)) return true;
			return false;
		}
	};

	/** @brief Adaptive spatial partition and render-node pool for a planet. */
	struct PlanetTerrainTree
	{
		/** @brief Half the root side length; root width is exactly 2^24 metres. */
		static constexpr double initial_root_half_extent = 8388608.0;
		/** @brief Smallest render-node side length, matching the former 32 m chunks. */
		static constexpr double minimum_node_width = 32.0;
		/** @brief Number of halvings from a 2^24 m root to a 32 m leaf. */
		static constexpr uint8 maximum_depth = 19;
		DynamicArray<PlanetTerrainNode> nodes;
		PlanetTerrainNodeId root_id = static_cast<PlanetTerrainNodeId>(-1);
		uint64 next_node_generation_id = 1;
	};

	/** @brief Terrain, streaming, and render state owned by one planet. */
	struct PlanetData
	{
		WorldBodyId world_body_id = static_cast<WorldBodyId>(-1);
		PlanetSettingsId settings_id = planet_settings_ids::earth;
		uint64 generation_id = 0;
		uint64 terrain_revision = 0;
		uint64 terrain_tree_revision = 0;
		bool terrain_edits_loaded = false;
		PlanetTerrainEdits terrain_edits;
		PlanetTerrainTree terrain_tree;
		DynamicArray<MeshGenerationRequest> pending_requests;

	};
}
