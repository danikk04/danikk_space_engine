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
		constexpr uint8 empty_edit_bucket = 0;
		constexpr uint8 occupied_edit_bucket = 1;
		constexpr uint8 deleted_edit_bucket = 2;

		bool sameEditGridCoordinate(const glm::ivec3& left, const glm::ivec3& right)
		{
			return left.x == right.x && left.y == right.y && left.z == right.z;
		}

		uint64 hashEditGridCoordinate(const glm::ivec3& coordinate)
		{
			uint64 hash = 1469598103934665603ull;
			for(int32 value : {coordinate.x, coordinate.y, coordinate.z})
			{
				hash ^= static_cast<uint32>(value);
				hash *= 1099511628211ull;
			}
			hash ^= hash >> 30;
			hash *= 0xbf58476d1ce4e5b9ull;
			hash ^= hash >> 27;
			hash *= 0x94d049bb133111ebull;
			return hash ^ (hash >> 31);
		}

		size_t findEditBucketForInsert(
			const DynamicArray<PlanetTerrainEditBucket>& buckets, const glm::ivec3& coordinate)
		{
			if(buckets.size() == 0) return static_cast<size_t>(-1);
			const size_t mask = buckets.size() - 1;
			size_t index = static_cast<size_t>(hashEditGridCoordinate(coordinate)) & mask;
			size_t first_deleted = static_cast<size_t>(-1);
			for(size_t checked = 0; checked < buckets.size(); ++checked)
			{
				const PlanetTerrainEditBucket& bucket = buckets[index];
				if(bucket.state == empty_edit_bucket)
					return first_deleted == static_cast<size_t>(-1) ? index : first_deleted;
				if(bucket.state == occupied_edit_bucket
					&& sameEditGridCoordinate(bucket.grid_coordinate, coordinate)) return index;
				if(bucket.state == deleted_edit_bucket && first_deleted == static_cast<size_t>(-1))
					first_deleted = index;
				index = (index + 1) & mask;
			}
			return first_deleted;
		}

		void growEditIndex(PlanetTerrainEditIndex& index)
		{
			if(index.buckets.size() != 0
				&& (index.used_bucket_count + 1) * 10 < index.buckets.size() * 7) return;
			const size_t new_capacity = index.buckets.size() == 0 ? 16 : index.buckets.size() * 2;
			DynamicArray<PlanetTerrainEditBucket> new_buckets;
			new_buckets.resize(new_capacity);
			for(PlanetTerrainEditBucket& old_bucket : index.buckets)
			{
				if(old_bucket.state != occupied_edit_bucket) continue;
				size_t bucket_index = static_cast<size_t>(hashEditGridCoordinate(old_bucket.grid_coordinate))
					& (new_capacity - 1);
				while(new_buckets[bucket_index].state == occupied_edit_bucket)
					bucket_index = (bucket_index + 1) & (new_capacity - 1);
				new_buckets[bucket_index] = static_cast<PlanetTerrainEditBucket&&>(old_bucket);
			}
			index.buckets = static_cast<DynamicArray<PlanetTerrainEditBucket>&&>(new_buckets);
			index.used_bucket_count = index.bucket_count;
		}
	}

	/** @copydoc PlanetTerrainEdit::isLargeForChunkIndex */
	bool PlanetTerrainEdit::isLargeForChunkIndex(double chunk_width) const
	{
		return radius > chunk_width * 0.5;
	}

	/** @copydoc PlanetTerrainEdit::getCenterGridCoordinate */
	glm::ivec3 PlanetTerrainEdit::getCenterGridCoordinate(double chunk_width) const
	{
		return glm::ivec3(
			static_cast<int32>(std::floor(center.x / chunk_width)),
			static_cast<int32>(std::floor(center.y / chunk_width)),
			static_cast<int32>(std::floor(center.z / chunk_width)));
	}

	/** @brief Returns the occupied hash-bucket index for a key, or an invalid sentinel. */
	size_t PlanetTerrainEditIndex::findBucket(const glm::ivec3& coordinate) const
	{
		if(buckets.size() == 0) return static_cast<size_t>(-1);
		const size_t mask = buckets.size() - 1;
		size_t index = static_cast<size_t>(hashEditGridCoordinate(coordinate)) & mask;
		for(size_t checked = 0; checked < buckets.size(); ++checked)
		{
			const PlanetTerrainEditBucket& bucket = buckets[index];
			if(bucket.state == empty_edit_bucket) return static_cast<size_t>(-1);
			if(bucket.state == occupied_edit_bucket
				&& sameEditGridCoordinate(bucket.grid_coordinate, coordinate)) return index;
			index = (index + 1) & mask;
		}
		return static_cast<size_t>(-1);
	}

	/** @brief Adds an edit to its center bucket or the separate large-edit list. */
	void PlanetTerrainEditIndex::insert(
		PlanetTerrainEditId edit_id, const PlanetTerrainEdit& edit, double chunk_width)
	{
		const PlanetTerrainEditReference reference{edit_id, next_order++};
		if(edit.isLargeForChunkIndex(chunk_width))
		{
			large_edits.push(reference);
			return;
		}
		growEditIndex(*this);
		const glm::ivec3 coordinate = edit.getCenterGridCoordinate(chunk_width);
		const size_t bucket_index = findEditBucketForInsert(buckets, coordinate);
		PlanetTerrainEditBucket& bucket = buckets[bucket_index];
		if(bucket.state != occupied_edit_bucket)
		{
			if(bucket.state == empty_edit_bucket) used_bucket_count++;
			bucket.grid_coordinate = coordinate;
			bucket.state = occupied_edit_bucket;
			bucket_count++;
		}
		bucket.edits.push(reference);
	}

	/** @brief Removes one edit while preserving the chronological order of its bucket. */
	bool PlanetTerrainEditIndex::remove(
		PlanetTerrainEditId edit_id, const PlanetTerrainEdit& edit, double chunk_width)
	{
		DynamicArray<PlanetTerrainEditReference>* references = &large_edits;
		size_t bucket_index = static_cast<size_t>(-1);
		if(!edit.isLargeForChunkIndex(chunk_width))
		{
			bucket_index = findBucket(edit.getCenterGridCoordinate(chunk_width));
			if(bucket_index == static_cast<size_t>(-1)) return false;
			references = &buckets[bucket_index].edits;
		}
		for(size_t index = 0; index < references->size(); ++index)
		{
			if((*references)[index].edit_id != edit_id) continue;
			for(size_t shift = index + 1; shift < references->size(); ++shift)
				(*references)[shift - 1] = (*references)[shift];
			references->resize(references->size() - 1);
			if(bucket_index != static_cast<size_t>(-1) && references->size() == 0)
			{
				buckets[bucket_index].state = deleted_edit_bucket;
				bucket_count--;
				if(bucket_count == 0)
				{
					buckets.clear();
					used_bucket_count = 0;
				}
			}
			return true;
		}
		return false;
	}

	/** @brief Clears all edit references and resets the chronological counter. */
	void PlanetTerrainEditIndex::clear()
	{
		buckets.clear();
		large_edits.clear();
		bucket_count = 0;
		used_bucket_count = 0;
		next_order = 0;
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

	/** @copydoc PlanetTerrainEdit::intersectsAabb */
	bool PlanetTerrainEdit::intersectsAabb(
		const glm::dvec3& minimum, const glm::dvec3& maximum, double expansion) const
	{
		const glm::dvec3 closest = glm::clamp(center, minimum, maximum);
		const double expanded_radius = radius + expansion;
		return glm::dot(center - closest, center - closest) <= expanded_radius * expanded_radius;
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
