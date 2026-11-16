#include <danikk_space_engine/component/world_body/planet/planet.h>

namespace danikk_space_engine
{
	namespace
	{
		constexpr uint8 empty_bucket = 0;
		constexpr uint8 occupied_bucket = 1;
		constexpr uint8 deleted_bucket = 2;

		/**
		 * @brief Compares all three integer coordinate components.
		 */
		bool sameChunkKey(const glm::ivec3& left, const glm::ivec3& right)
		{
			return left.x == right.x && left.y == right.y && left.z == right.z;
		}

		/**
		 * @brief Computes a deterministic 64-bit hash from all three signed grid coordinates for the chunk lookup table.
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
		 * @brief Uses linear probing to find a matching coordinate or the first available bucket.
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
		 * @brief Keeps the open-addressing table below its load threshold and rehashes occupied entries when it grows.
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
	 * @brief Clears desired flags and tombstones entries that no longer own loaded chunks.
	 */
	void PlanetChunkLookup::clearDesired()
	{
		for(PlanetChunkLookupEntry& entry : entries)
		{
			if(entry.state != occupied_bucket) continue;
			entry.desired = false;
			if(entry.has_record) continue;
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
	 * @brief Marks this coordinate as needed in the current streaming update.
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
	 * @brief Sets chunk.
	 */
	void PlanetChunkLookup::setChunk(const glm::ivec3& grid_coordinate, ChunkId chunk_id)
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
	}

	/**
	 * @brief Removes a loaded coordinate from the lookup table and clears its chunk record.
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
	 * @brief Reports whether the grid coordinate is in the current desired chunk set.
	 */
	bool PlanetChunkLookup::isDesired(const glm::ivec3& grid_coordinate) const
	{
		const size_t index = findChunkBucket(entries, grid_coordinate, false);
		return index != static_cast<size_t>(-1) && entries[index].desired;
	}

	/**
	 * @brief Reports whether generation has produced a chunk for the grid coordinate.
	 */
	bool PlanetChunkLookup::isGenerated(const glm::ivec3& grid_coordinate) const
	{
		const size_t index = findChunkBucket(entries, grid_coordinate, false);
		return index != static_cast<size_t>(-1) && entries[index].has_record;
	}

	/**
	 * @brief Returns a checked reference to an active chunk; an invalid ID raises a fatal error.
	 */
	bool PlanetChunkLookup::getChunk(const glm::ivec3& grid_coordinate, ChunkId& chunk_id) const
	{
		const size_t index = findChunkBucket(entries, grid_coordinate, false);
		if(index == static_cast<size_t>(-1)) return false;
		const PlanetChunkLookupEntry& entry = entries[index];
		if(!entry.has_record || entry.chunk_id == static_cast<ChunkId>(-1)) return false;
		chunk_id = entry.chunk_id;
		return true;
	}
}
