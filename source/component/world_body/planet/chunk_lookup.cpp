#include <danikk_space_engine/component/world_body/planet/planet.h>

namespace danikk_space_engine
{
	namespace
	{
		constexpr uint8 empty_bucket = 0;
		constexpr uint8 occupied_bucket = 1;
		constexpr uint8 deleted_bucket = 2;

		/**
		 * @brief Tests exact equality of two integer planet-grid coordinates.
		 * @param left First chunk coordinate.
		 * @param right Second chunk coordinate.
		 * @return True when x, y, and z all match.
		 */
		bool sameChunkKey(const glm::ivec3& left, const glm::ivec3& right)
		{
			return left.x == right.x && left.y == right.y && left.z == right.z;
		}

		/**
		 * @brief Hashes all signed coordinate components with a deterministic 64-bit mixing sequence.
		 * @param key Planet chunk coordinate to hash.
		 * @return Hash used as the initial bucket in the power-of-two open-addressed table.
		 */
		uint64 hashChunkKey(const glm::ivec3& key)
		{
			uint64 hash = 1469598103934665603ull;
			for(int32 coordinate : {key.x, key.y, key.z})
			{
				hash ^= static_cast<uint32>(coordinate);
				hash *= 1099511628211ull;
			}
			hash ^= hash >> 30;
			hash *= 0xbf58476d1ce4e5b9ull;
			hash ^= hash >> 27;
			hash *= 0x94d049bb133111ebull;
			return hash ^ (hash >> 31);
		}

		/**
		 * @brief Probes the table for a key, optionally preferring the first tombstone for insertion.
		 * @param entries Open-addressed bucket array; capacity must be a power of two.
		 * @param key Coordinate being searched or inserted.
		 * @param find_deleted_bucket Whether a deleted bucket can be returned as an insertion target.
		 * @return Matching/available bucket index, or the invalid-size sentinel if no bucket is usable.
		 */
		size_t findChunkBucket(
			const DynamicArray<PlanetChunkLookupEntry>& entries,
			const glm::ivec3& key,
			bool find_deleted_bucket)
		{
			if(entries.size() == 0) return static_cast<size_t>(-1);
			const size_t mask = entries.size() - 1;
			size_t index = static_cast<size_t>(hashChunkKey(key)) & mask;
			size_t first_deleted = static_cast<size_t>(-1);
			for(size_t checked = 0; checked < entries.size(); checked++)
			{
				const PlanetChunkLookupEntry& entry = entries[index];
				if(entry.state == empty_bucket)
				{
					return find_deleted_bucket && first_deleted != static_cast<size_t>(-1)
						? first_deleted
						: index;
				}
				if(entry.state == occupied_bucket && sameChunkKey(entry.grid_coordinate, key)) return index;
				if(find_deleted_bucket && entry.state == deleted_bucket && first_deleted == static_cast<size_t>(-1))
				{
					first_deleted = index;
				}
				index = (index + 1) & mask;
			}
			return first_deleted;
		}

		/**
		 * @brief Grows the lookup table at its load threshold and rehashes live entries.
		 *
		 * Capacity starts at 16 and doubles; tombstones are discarded during rehashing.
		 * @param lookup Table whose bucket storage may be replaced.
		 */
		void ensureChunkLookupCapacity(PlanetChunkLookup& lookup)
		{
			if(lookup.entries.size() != 0
				&& (lookup.used_bucket_count + 1) * 10 < lookup.entries.size() * 7) return;

			const size_t new_capacity = lookup.entries.size() == 0 ? 16 : lookup.entries.size() * 2;
			DynamicArray<PlanetChunkLookupEntry> new_entries;
			new_entries.resize(new_capacity);
			for(const PlanetChunkLookupEntry& old_entry : lookup.entries)
			{
				if(old_entry.state != occupied_bucket) continue;
				size_t index = static_cast<size_t>(hashChunkKey(old_entry.grid_coordinate)) & (new_capacity - 1);
				while(new_entries[index].state == occupied_bucket) index = (index + 1) & (new_capacity - 1);
				new_entries[index] = old_entry;
			}
			lookup.entries = static_cast<DynamicArray<PlanetChunkLookupEntry>&&>(new_entries);
			lookup.used_bucket_count = lookup.entry_count;
		}
	}

	/**
	 * @brief Starts a new desired-set pass while retaining loaded records as candidates for reuse.
	 *
	 * Every retained loaded coordinate has its desired flag cleared. Entries that have no completed
	 * generation record are tombstoned, since they represent obsolete pending/empty state.
	 */
	void PlanetChunkLookup::clearDesired()
	{
		for(PlanetChunkLookupEntry& entry : entries)
		{
			if(entry.state != occupied_bucket) continue;
			entry.desired = false;
			if(entry.has_record || entry.chunk_id != static_cast<ChunkId>(-1)) continue;
			entry.state = deleted_bucket;
			entry_count--;
		}
		if(entry_count == 0)
		{
			entries.clear();
			used_bucket_count = 0;
		}
	}

	/**
	 * @brief Ensures a lookup record exists and marks the coordinate wanted in this update pass.
	 * @param grid_coordinate Planet grid coordinate to retain or request.
	 */
	void PlanetChunkLookup::markDesired(const glm::ivec3& grid_coordinate)
	{
		ensureChunkLookupCapacity(*this);
		const size_t index = findChunkBucket(entries, grid_coordinate, true);
		PlanetChunkLookupEntry& entry = entries[index];
		if(entry.state != occupied_bucket)
		{
			if(entry.state == empty_bucket) used_bucket_count++;
			entry.grid_coordinate = grid_coordinate;
			entry.chunk_id = static_cast<ChunkId>(-1);
			entry.has_record = false;
			entry.desired = false;
			entry.state = occupied_bucket;
			entry_count++;
		}
		entry.desired = true;
	}

	/**
	 * @brief Stores a completed generation result, including a known-empty result with invalid chunk ID.
	 * @param grid_coordinate Coordinate represented by the completed result.
	 * @param chunk_id Loaded chunk ID, or the invalid sentinel when generation produced no geometry.
	 */
	void PlanetChunkLookup::setChunk(
		const glm::ivec3& grid_coordinate,
		ChunkId chunk_id,
		PlanetMeshGenerator mesh_generator)
	{
		ensureChunkLookupCapacity(*this);
		const size_t index = findChunkBucket(entries, grid_coordinate, true);
		PlanetChunkLookupEntry& entry = entries[index];
		if(entry.state != occupied_bucket)
		{
			if(entry.state == empty_bucket) used_bucket_count++;
			entry.grid_coordinate = grid_coordinate;
			entry.desired = false;
			entry.state = occupied_bucket;
			entry_count++;
		}
		entry.chunk_id = chunk_id;
		entry.has_record = true;
		entry.mesh_generator = mesh_generator;
	}

	/**
	 * @brief Clears the generated record and tombstones it unless the coordinate is still desired.
	 * @param grid_coordinate Coordinate whose loaded/generated record is being invalidated.
	 */
	void PlanetChunkLookup::remove(const glm::ivec3& grid_coordinate)
	{
		const size_t index = findChunkBucket(entries, grid_coordinate, false);
		if(index == static_cast<size_t>(-1)) return;
		PlanetChunkLookupEntry& entry = entries[index];
		entry.has_record = false;
		entry.chunk_id = static_cast<ChunkId>(-1);
		if(entry.desired) return;
		entry.state = deleted_bucket;
		entry_count--;
	}

	/**
	 * @brief Checks whether a coordinate was included in the latest desired-set calculation.
	 * @param grid_coordinate Coordinate to query.
	 * @return True if its live lookup entry has the desired flag set.
	 */
	bool PlanetChunkLookup::isDesired(const glm::ivec3& grid_coordinate) const
	{
		const size_t index = findChunkBucket(entries, grid_coordinate, false);
		return index != static_cast<size_t>(-1) && entries[index].desired;
	}

	/**
	 * @brief Checks whether generation completed for a coordinate, including an empty result.
	 * @param grid_coordinate Coordinate to query.
	 * @return True if a generated record exists, regardless of whether it owns a WorldData chunk.
	 */
	bool PlanetChunkLookup::isGenerated(
		const glm::ivec3& grid_coordinate,
		PlanetMeshGenerator mesh_generator) const
	{
		const size_t index = findChunkBucket(entries, grid_coordinate, false);
		return index != static_cast<size_t>(-1)
			&& entries[index].has_record
			&& entries[index].mesh_generator == mesh_generator;
	}

	/**
	 * @brief Retrieves a loaded chunk ID for a coordinate without treating known-empty as loaded.
	 * @param grid_coordinate Coordinate to query.
	 * @param chunk_id Receives the associated chunk ID when the lookup has one.
	 * @return True if a record has a non-sentinel chunk ID; false for absent or empty results.
	 */
	bool PlanetChunkLookup::getChunk(const glm::ivec3& grid_coordinate, ChunkId& chunk_id) const
	{
		const size_t index = findChunkBucket(entries, grid_coordinate, false);
		if(index == static_cast<size_t>(-1)) return false;
		const PlanetChunkLookupEntry& entry = entries[index];
		if(entry.chunk_id == static_cast<ChunkId>(-1)) return false;
		chunk_id = entry.chunk_id;
		return true;
	}
}
