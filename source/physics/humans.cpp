#include <algorithm>
#include <cmath>
#include <limits>

#include <danikk_space_engine/profiler.h>
#include <danikk_space_engine/physics.h>
#include <danikk_space_engine/component/point/damageable.h>
#include <danikk_space_engine/component/point/human/human.h>
#include <danikk_space_engine/planet_system.h>
#include <danikk_space_engine/world/world.h>

namespace danikk_space_engine
{
	namespace
	{
		constexpr double minimum_ground_normal_dot = 0.2;

		/**
		 * @brief Stores a human capsule’s world position, up axis, and dimensions for collision resolution.
		 */
		struct HumanCollider
		{
			PointId point_id;
			WorldBodyId body_id;
			Chunk* chunk;
			HumanComponent* component;
			glm::dvec3 center;
			glm::dvec3 up;
			double height;
			double radius;
			glm::dvec3 axis_start;
			glm::dvec3 axis_end;
			glm::dquat world_to_body_rotation;
		};

		/** @brief Looks up a human payload from its owning chunk's cache of human slot IDs. */
		HumanComponent& getChunkHuman(PointData& data, PointComponentSlotId slot_id)
		{
			return *static_cast<HumanComponent*>(data.getPointComponentSlot(slot_id).getData());
		}

		/**
		 * @brief Samples human capsules after component and body motion and marks changes for the solver.
		 * The world transform detects motion inherited from the owning body. The chunk ID also
		 * catches reattachment without a pose change, which can change the candidate chunk pairs.
		 */
		void updateHumanCollisionActivity(PointData& data, Chunk& chunk, const WorldBody& body)
		{
			chunk.has_active_human_collisions = false;
			for(PointComponentSlotId slot_id : chunk.getCollisionHumanComponentSlotIds())
			{
				PointComponentSlot& slot = data.getPointComponentSlot(slot_id);
				auto& human = *static_cast<HumanComponent*>(slot.getData());
				auto& activity = human.collision_activity;
				human.displaced_by_human_collision = false;
				const float height = human.getHeight();
				if(height <= 0.0f || !data.isPointActive(slot.point_id))
				{
					activity.chunk_id = PointComponentSlot::no_chunk;
					activity.active = false;
					continue;
				}

				const glm::dvec3 local_position = chunk.position + glm::dvec3(data.getPoint(slot.point_id).position);
				const glm::dvec3 center = body.position + body.rotation * local_position;
				const glm::dvec3 up = body.rotation * glm::dvec3(human.up);
				const bool active = activity.recheck_next_tick || activity.chunk_id != chunk.chunk_id
					|| activity.height != height || activity.center != center || activity.up != up;
				activity = {center, up, chunk.chunk_id, height, active, false};
				chunk.has_active_human_collisions |= active;
			}
		}

		/** @brief An occupied chunk and its capsule bounds for queries outside the planet grid. */
		struct OccupiedHumanChunk
		{
			const Chunk* chunk;
			const WorldBody* body;
			const PlanetData* grid_planet;
			glm::dvec3 minimum;
			glm::dvec3 maximum;
		};

		/** @brief Builds one collider on the stack from an active human component slot. */
		bool getHumanCollider(World& world, PointComponentSlotId slot_id, HumanCollider& result)
		{
			if(!world.point_data.isPointComponentSlotActive(slot_id)) return false;
			PointComponentSlot& slot = world.point_data.getPointComponentSlot(slot_id);
			if(slot.type_id != PointComponentTypeId::human || !world.chunks.isChunkActive(slot.chunk_id)
				|| !world.point_data.isPointActive(slot.point_id)) return false;
			HumanComponent& component = *static_cast<HumanComponent*>(slot.getData());
			if(component.hasDeathTimestamp()) return false;
			const DamageableComponent* health = findDamageable(world, slot.chunk_id, slot.point_id);
			if(health != nullptr && health->getHealth() <= 0.0f) return false;
			const double height = component.getHeight();
			if(height <= 0.0) return false;

			Chunk& chunk = world.chunks.getChunk(slot.chunk_id);
			const WorldBody& body = world.world_body_data.getWorldBody(chunk.world_body_id);
			const glm::dvec3 up = glm::normalize(body.rotation * glm::dvec3(glm::normalize(component.up)));
			const double radius = height * 0.14;
			const glm::dvec3 center = world.getPointWorldPosition(slot.chunk_id, slot.point_id);
			const double half_axis = height * 0.5 - radius;
			result = HumanCollider{
				slot.point_id,
				chunk.world_body_id,
				&chunk,
				&component,
				center,
				up,
				height,
				radius,
				center - up * half_axis,
				center + up * half_axis,
				glm::conjugate(body.rotation)};
			return true;
		}

		/**
		 * @brief Computes closest points on two finite capsule-axis segments.
		 * @param a0 Start of the first segment.
		 * @param a1 End of the first segment.
		 * @param b0 Start of the second segment.
		 * @param b1 End of the second segment.
		 * @param closest_a Receives the closest point on segment a.
		 * @param closest_b Receives the closest point on segment b.
		 * Handles two zero-length segments and either single degenerate segment,
		 * then clamps the closest-point parameters to both finite segments.
		 */
		void closestSegmentPoints(const glm::dvec3& a0, const glm::dvec3& a1,
			const glm::dvec3& b0, const glm::dvec3& b1,
			glm::dvec3& closest_a, glm::dvec3& closest_b)
		{
			const glm::dvec3 direction_a = a1 - a0;
			const glm::dvec3 direction_b = b1 - b0;
			const glm::dvec3 between_starts = a0 - b0;
			const double a_length_squared = glm::dot(direction_a, direction_a);
			const double b_length_squared = glm::dot(direction_b, direction_b);
			const double b_dot = glm::dot(direction_a, direction_b);
			const double a_dot = glm::dot(direction_a, between_starts);
			const double b_offset = glm::dot(direction_b, between_starts);
			double parameter_a = 0.0;
			double parameter_b = 0.0;

			if(a_length_squared <= 1e-12 && b_length_squared <= 1e-12)
			{
				closest_a = a0;
				closest_b = b0;
				return;
			}
			if(a_length_squared <= 1e-12)
			{
				parameter_b = glm::clamp(b_offset / b_length_squared, 0.0, 1.0);
			}
			else if(b_length_squared <= 1e-12)
			{
				parameter_a = glm::clamp(-a_dot / a_length_squared, 0.0, 1.0);
			}
			else
			{
				const double denominator = a_length_squared * b_length_squared - b_dot * b_dot;
				if(denominator > 1e-12)
					parameter_a = glm::clamp((b_dot * b_offset - a_dot * b_length_squared) / denominator, 0.0, 1.0);
				parameter_b = (b_dot * parameter_a + b_offset) / b_length_squared;
				if(parameter_b < 0.0)
				{
					parameter_b = 0.0;
					parameter_a = glm::clamp(-a_dot / a_length_squared, 0.0, 1.0);
				}
				else if(parameter_b > 1.0)
				{
					parameter_b = 1.0;
					parameter_a = glm::clamp((b_dot - a_dot) / a_length_squared, 0.0, 1.0);
				}
			}

			closest_a = a0 + direction_a * parameter_a;
			closest_b = b0 + direction_b * parameter_b;
		}

		/**
		 * @brief Applies a world-space displacement to a human's point in its body's local frame.
		 * @param world World providing the owning chunk, body, and point.
		 * @param human Collider record whose center and point position are updated.
		 * @param world_delta Requested displacement in world coordinates.
		 * @return True if the change survived rounding to the point's local coordinates.
		 */
		bool moveHuman(World& world, HumanCollider& human, const glm::dvec3& world_delta)
		{
			Point& point = world.point_data.getPoint(human.point_id);
			const vec3 old_position = point.position;
			point.position += vec3(human.world_to_body_rotation * world_delta);
			if(point.position == old_position) return false;

			const glm::dvec3 local_delta = glm::dvec3(point.position) - glm::dvec3(old_position);
			const glm::dvec3 applied_delta = glm::conjugate(human.world_to_body_rotation) * local_delta;
			human.component->displaced_by_human_collision = true;
			human.component->collision_activity.active = true;
			human.component->collision_activity.recheck_next_tick = true;
			human.chunk->has_active_human_collisions = true;
			human.center += applied_delta;
			human.axis_start += applied_delta;
			human.axis_end += applied_delta;
			return true;
		}

		/**
		 * @brief Uses a short downward raycast to lift a separated human back above nearby ground.
		 * @param world World used for raycasting and point/body access.
		 * @param human Human capsule to check and possibly move.
		 * The ray only queries the human's WorldBody. The hit normal must have at least
		 * a 0.2 dot product with the human up axis. Only upward corrections from
		 * 0.1 mm through 1 m are applied; this helper never snaps the human downward.
		 */
		void restoreHumanGroundClearance(World& world, HumanCollider& human)
		{
			DSE_WATCH();
			const glm::dvec3 foot_position = human.center - human.up * (human.height * 0.5);
			constexpr double probe_height = 1.0;
			constexpr double maximum_lift = 1.0;
			const RaycastHit hit = world.raycastBody(
				human.body_id,
				foot_position + human.up * probe_height,
				vec3(-human.up),
				static_cast<float>(probe_height + maximum_lift));
			if(!hit.hit || glm::dot(glm::dvec3(hit.normal), human.up) < minimum_ground_normal_dot) return;

			const glm::dvec3 supported_center = hit.position + human.up * (human.height * 0.5 + 0.02);
			const glm::dvec3 correction = supported_center - human.center;
			const double lift = glm::dot(correction, human.up);
			if(lift <= 0.0001 || lift > maximum_lift) return;
			moveHuman(world, human, correction);
		}

		/** @brief Applies one capsule-pair positional correction and reports whether either human moved. */
		bool resolveHumanPair(World& world, HumanCollider& a, HumanCollider& b)
		{
			const double maximum_distance = (a.height + b.height) * 0.5;
			const glm::dvec3 center_delta = b.center - a.center;
			if(glm::dot(center_delta, center_delta) > maximum_distance * maximum_distance) return false;

			glm::dvec3 closest_a;
			glm::dvec3 closest_b;
			closestSegmentPoints(a.axis_start, a.axis_end, b.axis_start, b.axis_end, closest_a, closest_b);
			glm::dvec3 normal = closest_b - closest_a;
			const double radius_sum = a.radius + b.radius;
			const double distance_squared = glm::dot(normal, normal);
			if(distance_squared >= radius_sum * radius_sum) return false;
			const double distance = std::sqrt(distance_squared);
			const double penetration = radius_sum - distance;

			if(distance > 1e-9)
			{
				normal /= distance;
			}
			else
			{
				glm::dvec3 average_up = a.up + b.up;
				if(glm::length(average_up) <= 1e-9) average_up = a.up;
				average_up = glm::normalize(average_up);
				normal = center_delta - average_up * glm::dot(center_delta, average_up);
				if(glm::length(normal) <= 1e-9)
				{
					const glm::dvec3 axis = std::abs(average_up.x) < 0.8
						? glm::dvec3(1.0, 0.0, 0.0) : glm::dvec3(0.0, 1.0, 0.0);
					normal = glm::cross(average_up, axis);
				}
				normal = glm::normalize(normal);
			}

			glm::dvec3 move_a = -normal - a.up * glm::dot(-normal, a.up);
			glm::dvec3 move_b = normal - b.up * glm::dot(normal, b.up);
			const double move_a_factor = glm::length(move_a);
			const double move_b_factor = glm::length(move_b);
			const double total_factor = move_a_factor + move_b_factor;
			if(total_factor <= 1e-9)
			{
				a.component->collision_activity.recheck_next_tick = true;
				b.component->collision_activity.recheck_next_tick = true;
				return false;
			}
			const double correction_distance = penetration / total_factor;
			bool corrected = false;
			if(move_a_factor > 1e-9)
				corrected |= moveHuman(world, a, glm::normalize(move_a) * correction_distance);
			if(move_b_factor > 1e-9)
				corrected |= moveHuman(world, b, glm::normalize(move_b) * correction_distance);
			return corrected;
		}

		/**
		 * @brief Resolves the human pairs from two chunks using their cached slot IDs.
		 * @return True when at least one pair was separated.
		 * Idle pairs are skipped before constructing colliders. For a chunk paired with itself,
		 * the inner loop starts after the outer slot.
		 */
		bool resolveHumanChunkPair(World& world, const Chunk& first, const Chunk& second)
		{
			if(!first.has_active_human_collisions && !second.has_active_human_collisions) return false;
			const auto& first_ids = first.getCollisionHumanComponentSlotIds();
			const auto& second_ids = second.getCollisionHumanComponentSlotIds();
			const bool same_chunk = first.chunk_id == second.chunk_id;
			if(first_ids.size() == 0 || second_ids.size() <= (same_chunk ? 1u : 0u)) return false;

			bool corrected = false;
			const size_t first_count = first_ids.size() - (same_chunk ? 1u : 0u);
			for(size_t a_index = 0; a_index < first_count; ++a_index)
			{
				const auto& activity = getChunkHuman(world.point_data, first_ids[a_index]).collision_activity;
				if(!activity.active && !second.has_active_human_collisions) continue;

				HumanCollider a;
				bool has_collider = false;

				const size_t start = same_chunk ? a_index + 1 : 0;
				for(size_t b_index = start; b_index < second_ids.size(); ++b_index)
				{
					if(!activity.active && !getChunkHuman(world.point_data, second_ids[b_index]).collision_activity.active)
						continue;
					if(!has_collider)
					{
						if(!getHumanCollider(world, first_ids[a_index], a)) break;
						has_collider = true;
					}

					HumanCollider b;
					if(!getHumanCollider(world, second_ids[b_index], b) || a.point_id == b.point_id) continue;
					if(resolveHumanPair(world, a, b)) corrected = true;
				}
			}
			return corrected;
		}

		/** @brief Visits one half of the 26 neighboring cells so each grid chunk pair is resolved once. */
		bool resolvePlanetHumanChunkNeighbors(World& world, const OccupiedHumanChunk& entry)
		{
			bool corrected = false;
			const Chunk& chunk = *entry.chunk;
			for(int32 z = 0; z <= 1; ++z)
				for(int32 y = -1; y <= 1; ++y)
					for(int32 x = -1; x <= 1; ++x)
					{
						if(z == 0 && (y < 0 || (y == 0 && x <= 0))) continue;
						const glm::ivec3 coordinate = chunk.grid_coordinate + glm::ivec3(x, y, z);
						ChunkId neighbor_id;
						if(!entry.grid_planet->chunk_lookup.getChunk(coordinate, neighbor_id)
							|| !world.chunks.isChunkActive(neighbor_id)) continue;

						const Chunk& neighbor = world.chunks.getChunk(neighbor_id);
						if(neighbor.world_body_id == chunk.world_body_id && neighbor.grid_coordinate == coordinate)
							corrected |= resolveHumanChunkPair(world, chunk, neighbor);
					}
			return corrected;
		}

		/**
		 * @brief Finds overlapping occupied chunks when a shared planet grid is unavailable.
		 * @return True when resolving these pairs moved at least one human.
		 * Builds world-space bounds from the current capsule centers and half heights, then
		 * sorts along the widest axis to skip separated chunks. A new pass rebuilds the bounds
		 * if a correction moved humans during this pass.
		 */
		bool resolveHumanChunksOutsideSharedGrid(World& world, DynamicArray<OccupiedHumanChunk>& chunks)
		{
			const glm::dvec3 infinity(std::numeric_limits<double>::infinity());
			glm::dvec3 minimum = infinity;
			glm::dvec3 maximum = -infinity;
			for(OccupiedHumanChunk& entry : chunks)
			{
				entry.minimum = infinity;
				entry.maximum = -infinity;
				for(PointComponentSlotId slot_id : entry.chunk->getCollisionHumanComponentSlotIds())
				{
					const PointComponentSlot& slot = world.point_data.getPointComponentSlot(slot_id);
					const auto& human = *static_cast<const HumanComponent*>(slot.getData());
					const double half_height = human.getHeight() * 0.5;
					if(half_height <= 0.0 || !world.point_data.isPointActive(slot.point_id)) continue;

					const glm::dvec3 local_position = entry.chunk->position
						+ glm::dvec3(world.point_data.getPoint(slot.point_id).position);
					const glm::dvec3 center = entry.body->position + entry.body->rotation * local_position;
					entry.minimum = glm::min(entry.minimum, center - glm::dvec3(half_height));
					entry.maximum = glm::max(entry.maximum, center + glm::dvec3(half_height));
				}
				minimum = glm::min(minimum, entry.minimum);
				maximum = glm::max(maximum, entry.maximum);
			}
			if(minimum.x > maximum.x) return false;

			const glm::dvec3 span = maximum - minimum;
			int axis = span.y > span.x ? 1 : 0;
			if(span.z > span[axis]) axis = 2;
			std::sort(chunks.begin(), chunks.end(), [axis](const OccupiedHumanChunk& first, const OccupiedHumanChunk& second)
			{
				if(first.minimum[axis] != second.minimum[axis]) return first.minimum[axis] < second.minimum[axis];
				return first.chunk->chunk_id < second.chunk->chunk_id;
			});

			bool corrected = false;
			for(size_t i = 0; i < chunks.size(); ++i)
			{
				const OccupiedHumanChunk& first = chunks[i];
				for(size_t j = i + 1; j < chunks.size(); ++j)
				{
					const OccupiedHumanChunk& second = chunks[j];
					if(second.minimum[axis] > first.maximum[axis]) break;
					if(first.grid_planet != nullptr && first.grid_planet == second.grid_planet) continue;
					if(glm::any(glm::greaterThan(first.minimum, second.maximum))
						|| glm::any(glm::greaterThan(second.minimum, first.maximum))) continue;

					corrected |= resolveHumanChunkPair(world, *first.chunk, *second.chunk);
				}
			}
			return corrected;
		}
	}

	/**
	 * @brief Separates overlapping human capsules in up to four passes over occupied chunk pairs.
	 * @param world World whose human point positions are adjusted.
	 * Each pass handles pairs within each chunk, then uses the shared planet grid or world-space
	 * capsule bounds to find neighboring chunks. Pairs of unchanged capsules are skipped
	 * before construction. After all passes, a short raycast restores clearance for displaced humans.
	 */
	void PhysicsSystem::resolveHumanCollisions()
	{
		DSE_WATCH();
		DynamicArray<OccupiedHumanChunk> occupied_chunks;
		bool has_chunk_outside_grid = false;
		bool has_multiple_bodies = false;
		bool has_active_humans = false;

		for(ChunkId chunk_id : world.chunks.getUsedChunkIds())
		{
			Chunk& chunk = world.chunks.getChunk(chunk_id);
			if(chunk.getCollisionHumanComponentSlotIds().size() == 0) continue;

			const WorldBody& body = world.world_body_data.getWorldBody(chunk.world_body_id);
			updateHumanCollisionActivity(world.point_data, chunk, body);
			has_active_humans |= chunk.has_active_human_collisions;
			const PlanetData* grid_planet = world.planets.findPlanet(chunk.world_body_id);
			ChunkId indexed_id;
			if(grid_planet != nullptr && (!grid_planet->chunk_lookup.getChunk(chunk.grid_coordinate, indexed_id)
				|| indexed_id != chunk_id))
				grid_planet = nullptr;

			has_chunk_outside_grid |= grid_planet == nullptr;
			if(occupied_chunks.size() > 0) has_multiple_bodies |= &body != occupied_chunks[0].body;
			occupied_chunks.push({&chunk, &body, grid_planet, glm::dvec3(0.0), glm::dvec3(0.0)});
		}
		if(!has_active_humans) return;

		const bool use_world_bounds = occupied_chunks.size() > 1 && (has_chunk_outside_grid || has_multiple_bodies);
		for(uint8 iteration = 0; iteration < 4; ++iteration)
		{
			bool corrected = false;
			for(const OccupiedHumanChunk& entry : occupied_chunks)
			{
				corrected |= resolveHumanChunkPair(world, *entry.chunk, *entry.chunk);
				if(occupied_chunks.size() > 1 && entry.grid_planet != nullptr)
					corrected |= resolvePlanetHumanChunkNeighbors(world, entry);
			}

			if(use_world_bounds) corrected |= resolveHumanChunksOutsideSharedGrid(world, occupied_chunks);
			if(!corrected) break;
		}

		for(const OccupiedHumanChunk& entry : occupied_chunks)
		{
			for(PointComponentSlotId slot_id : entry.chunk->getCollisionHumanComponentSlotIds())
			{
				auto& human = getChunkHuman(world.point_data, slot_id);
				if(!human.displaced_by_human_collision) continue;

				HumanCollider collider;
				if(getHumanCollider(world, slot_id, collider)) restoreHumanGroundClearance(world, collider);
				human.displaced_by_human_collision = false;
			}
		}
	}
}
