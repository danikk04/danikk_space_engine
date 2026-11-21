#include <algorithm>
#include <cmath>
#include <istream>
#include <ostream>

#include <danikk_space_engine/material.h>
#include <danikk_space_engine/systems/planet/system.h>

namespace danikk_space_engine
{
	namespace
	{
		constexpr uint32 invalid_node_id = static_cast<uint32>(-1);
		constexpr size_t max_edits_per_leaf = PlanetTerrainEdits::split_edit_threshold;

		uint8 findContainingChild(const PlanetTerrainEditNode& node, const PlanetTerrainEdit& edit)
		{
			const glm::dvec3 minimum = edit.getCenter() - glm::dvec3(edit.getRadius());
			const glm::dvec3 maximum = edit.getCenter() + glm::dvec3(edit.getRadius());
			uint8 child_index = 0;
			for(uint8 axis = 0; axis < 3; ++axis)
			{
				if(maximum[axis] <= node.center[axis]) continue;
				if(minimum[axis] >= node.center[axis]) child_index |= static_cast<uint8>(1u << axis);
				else return static_cast<uint8>(-1);
			}
			return child_index;
		}

		uint32 appendNode(PlanetTerrainEdits& tree, const glm::dvec3& center, double half_extent)
		{
			const uint32 node_id = static_cast<uint32>(tree.nodes.size());
			tree.nodes.pushCtor();
			tree.nodes[node_id].center = center;
			tree.nodes[node_id].half_extent = half_extent;
			return node_id;
		}

		void createChildren(PlanetTerrainEdits& tree, uint32 node_id)
		{
			if(tree.nodes[node_id].hasChildren()) return;
			const glm::dvec3 center = tree.nodes[node_id].center;
			const double child_half_extent = tree.nodes[node_id].half_extent * 0.5;
			uint32 child_ids[8];
			for(uint8 index = 0; index < 8; ++index)
			{
				const glm::dvec3 offset(
					(index & 1) ? child_half_extent : -child_half_extent,
					(index & 2) ? child_half_extent : -child_half_extent,
					(index & 4) ? child_half_extent : -child_half_extent);
				child_ids[index] = appendNode(tree, center + offset, child_half_extent);
			}
			for(uint8 index = 0; index < 8; ++index)
				tree.nodes[node_id].children[index] = child_ids[index];
		}

		void recordReferenceLocation(PlanetTerrainEdits& tree, uint32 node_id,
			const PlanetTerrainEditReference& reference)
		{
			if(tree.locations.size() <= reference.edit_id)
				tree.locations.resize(static_cast<size_t>(reference.edit_id) + 1);
			PlanetTerrainEditLocation& location = tree.locations[reference.edit_id];
			location.node_id = node_id;
			location.entry_index = tree.nodes[node_id].edits.size() - 1;
			location.order = reference.order;
		}

		void insertReference(PlanetTerrainEdits& tree, uint32 node_id,
			const PlanetTerrainEditReference& reference, const PlanetTerrainEdit& edit)
		{
			PlanetTerrainEditNode& node = tree.nodes[node_id];
			if(node.hasChildren())
			{
				const uint8 child_index = findContainingChild(node, edit);
				if(child_index != static_cast<uint8>(-1))
				{
					insertReference(tree, node.children[child_index], reference, edit);
					return;
				}
				tree.nodes[node_id].edits.push(reference);
				recordReferenceLocation(tree, node_id, reference);
				return;
			}
			if(node.edits.size() >= max_edits_per_leaf
				&& node.half_extent > PlanetTerrainEdits::minimum_cell_size * 0.5)
			{
				createChildren(tree, node_id);
				DynamicArray<PlanetTerrainEditReference> previous_edits =
					static_cast<DynamicArray<PlanetTerrainEditReference>&&>(tree.nodes[node_id].edits);
				for(const PlanetTerrainEditReference& previous : previous_edits)
					insertReference(tree, node_id, previous, (*tree.pool)[previous.edit_id]);
				insertReference(tree, node_id, reference, edit);
				return;
			}
			tree.nodes[node_id].edits.push(reference);
			recordReferenceLocation(tree, node_id, reference);
		}

		void expandRootToContain(PlanetTerrainEdits& tree, const PlanetTerrainEdit& edit)
		{
			if(tree.root_id == invalid_node_id)
				tree.root_id = appendNode(tree, glm::dvec3(0.0),
					PlanetTerrainEdits::initial_root_half_extent);
			const glm::dvec3 minimum = edit.getCenter() - glm::dvec3(edit.getRadius());
			const glm::dvec3 maximum = edit.getCenter() + glm::dvec3(edit.getRadius());
			while(true)
			{
				const PlanetTerrainEditNode& root = tree.nodes[tree.root_id];
				const glm::dvec3 root_minimum = root.center - glm::dvec3(root.half_extent);
				const glm::dvec3 root_maximum = root.center + glm::dvec3(root.half_extent);
				if(glm::all(glm::greaterThanEqual(minimum, root_minimum))
					&& glm::all(glm::lessThanEqual(maximum, root_maximum))) return;
				const glm::dvec3 old_center = root.center;
				const double old_half_extent = root.half_extent;
				glm::dvec3 shift(0.0);
				for(uint8 axis = 0; axis < 3; ++axis)
					shift[axis] = (edit.getCenter()[axis] >= old_center[axis] ? 1.0 : -1.0) * old_half_extent;
				const uint32 old_root_id = tree.root_id;
				const uint32 new_root_id = appendNode(tree, old_center + shift, old_half_extent * 2.0);
				uint8 old_child_index = 0;
				for(uint8 axis = 0; axis < 3; ++axis)
					if(old_center[axis] >= tree.nodes[new_root_id].center[axis])
						old_child_index |= static_cast<uint8>(1u << axis);
				tree.nodes[new_root_id].children[old_child_index] = old_root_id;
				tree.root_id = new_root_id;
			}
		}
	}

	PlanetTerrainEditId PlanetTerrainEdits::append(const PlanetTerrainEdit& edit)
	{
		const PlanetTerrainEditId edit_id = pool->create();
		(*pool)[edit_id] = edit;
		ids.push(edit_id);
		const uint64 order = next_order++;
		expandRootToContain(*this, edit);
		insertReference(*this, root_id, PlanetTerrainEditReference{edit_id, order}, edit);
		return edit_id;
	}

	bool PlanetTerrainEdits::remove(PlanetTerrainEditId edit_id)
	{
		if(!pool->isActive(edit_id) || edit_id >= locations.size()) return false;
		const PlanetTerrainEditLocation location = locations[edit_id];
		if(location.node_id >= nodes.size() || location.entry_index >= nodes[location.node_id].edits.size())
			return false;
		DynamicArray<PlanetTerrainEditReference>& entries = nodes[location.node_id].edits;
		for(size_t index = location.entry_index + 1; index < entries.size(); ++index)
		{
			entries[index - 1] = entries[index];
			locations[entries[index - 1].edit_id].entry_index = index - 1;
		}
		entries.resize(entries.size() - 1);
		locations[edit_id] = PlanetTerrainEditLocation{};
		for(size_t index = 0; index < ids.size(); ++index)
		{
			if(ids[index] != edit_id) continue;
			for(size_t shift = index + 1; shift < ids.size(); ++shift)
				ids[shift - 1] = ids[shift];
			ids.resize(ids.size() - 1);
			break;
		}
		return pool->free(edit_id);
	}

	/** @copydoc PlanetTerrainEdit::PlanetTerrainEdit */
	PlanetTerrainEdit::PlanetTerrainEdit(
		const glm::dvec3& center, double radius, MaterialId material_id)
		: center(center), radius(radius), material_id(material_id)
	{
	}

	/** @copydoc PlanetTerrainEdit::applyToScalarField */
	double PlanetTerrainEdit::applyToScalarField(const glm::dvec3& position, double density) const
	{
		const double distance = glm::length(position - center);
		return material_id == default_materials::void_material
			? std::min(density, distance - radius)
			: std::max(density, radius - distance);
	}

	/** @copydoc PlanetTerrainEdit::applyToSurfaceRadius */
	double PlanetTerrainEdit::applyToSurfaceRadius(const glm::dvec3& direction, double surface_radius) const
	{
		const double along_ray = glm::dot(center, direction);
		const double perpendicular_squared = glm::max(
			0.0, glm::dot(center, center) - along_ray * along_ray);
		if(perpendicular_squared >= radius * radius) return surface_radius;
		const double half_chord = std::sqrt(radius * radius - perpendicular_squared);
		const double outer_radius = along_ray + half_chord;
		const double inner_radius = glm::max(0.0, along_ray - half_chord);
		if(material_id != default_materials::void_material) return glm::max(surface_radius, outer_radius);
		if(surface_radius >= inner_radius && surface_radius <= outer_radius) return inner_radius;
		return surface_radius;
	}

	/** @copydoc PlanetTerrainEdit::applyToMaterial */
	MaterialId PlanetTerrainEdit::applyToMaterial(
		const glm::dvec3& position, MaterialId current_material) const
	{
		if(material_id == default_materials::void_material) return current_material;
		return glm::length(position - center) <= radius ? material_id : current_material;
	}

	/** @copydoc PlanetTerrainEdit::getMaximumRadiusFromOrigin */
	double PlanetTerrainEdit::getMaximumRadiusFromOrigin(double margin) const
	{
		return material_id != default_materials::void_material
			? glm::length(center) + radius + margin : 0.0;
	}

	/** @copydoc PlanetTerrainEdit::read */
	bool PlanetTerrainEdit::read(std::istream& input, uint32 format_version)
	{
		glm::dvec3 loaded_center;
		double loaded_radius = 0.0;
		MaterialId loaded_material_id = default_materials::void_material;
		input.read(reinterpret_cast<char*>(&loaded_center.x), sizeof(double));
		input.read(reinterpret_cast<char*>(&loaded_center.y), sizeof(double));
		input.read(reinterpret_cast<char*>(&loaded_center.z), sizeof(double));
		input.read(reinterpret_cast<char*>(&loaded_radius), sizeof(double));
		if(format_version == 1)
		{
			uint8 loaded_adds_solid_terrain = 0;
			input.read(reinterpret_cast<char*>(&loaded_adds_solid_terrain), sizeof(loaded_adds_solid_terrain));
			if(loaded_adds_solid_terrain > 1) return false;
			loaded_material_id = loaded_adds_solid_terrain
				? default_materials::stone : default_materials::void_material;
		}
		else if(format_version == 2)
			input.read(reinterpret_cast<char*>(&loaded_material_id), sizeof(loaded_material_id));
		else return false;
		if(!input || !std::isfinite(loaded_radius) || loaded_radius <= 0.0
			|| (loaded_material_id != default_materials::void_material
				&& loaded_material_id >= getMaterialCount())
			|| !std::isfinite(loaded_center.x) || !std::isfinite(loaded_center.y)
			|| !std::isfinite(loaded_center.z)) return false;
		center = loaded_center;
		radius = loaded_radius;
		material_id = loaded_material_id;
		return true;
	}

	/** @copydoc PlanetTerrainEdit::write */
	void PlanetTerrainEdit::write(std::ostream& output) const
	{
		output.write(reinterpret_cast<const char*>(&center.x), sizeof(double));
		output.write(reinterpret_cast<const char*>(&center.y), sizeof(double));
		output.write(reinterpret_cast<const char*>(&center.z), sizeof(double));
		output.write(reinterpret_cast<const char*>(&radius), sizeof(double));
		output.write(reinterpret_cast<const char*>(&material_id), sizeof(material_id));
	}
}
