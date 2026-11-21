#include <cmath>
#include <algorithm>

#include <danikk_space_engine/profiler.h>
#include <danikk_space_engine/systems/physics/physics.h>
#include <danikk_space_engine/component/point/damageable.h>
#include <danikk_space_engine/component/point/human/human.h>
#include <danikk_space_engine/systems/planet/system.h>
#include <danikk_space_engine/world.h>

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
			HumanComponent* component;
			glm::dvec3 center;
			glm::dvec3 up;
			double height;
			double radius;
			glm::dvec3 axis_start;
			glm::dvec3 axis_end;
			glm::dquat world_to_body_rotation;
		};

		/** @brief Conservative world-space bounds used to prune collisions across bodies. */
		struct HumanColliderBounds
		{
			PointComponentSlotId slot_id;
			WorldBodyId body_id;
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
			const DamageableComponent* health = findDamageable(world, slot.point_id);
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
			const RaycastHit hit = world.raycasts.castBody(
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

		/** @brief Resolves cross-body capsule pairs using a sweep over conservative bounds. */
		bool resolveCrossBodyHumanPairs(World& world,
			const DynamicArray<PointComponentSlotId>& human_slot_ids)
		{
			DynamicArray<HumanColliderBounds> bounds;
			for(PointComponentSlotId slot_id : human_slot_ids)
			{
				HumanCollider collider;
				if(!getHumanCollider(world, slot_id, collider)) continue;
				const glm::dvec3 extent(collider.height * 0.5);
				bounds.push(HumanColliderBounds{
					slot_id, collider.body_id, collider.center - extent, collider.center + extent});
			}
			std::sort(bounds.begin(), bounds.end(), [](const auto& left, const auto& right)
			{
				if(left.minimum.x != right.minimum.x) return left.minimum.x < right.minimum.x;
				return left.slot_id < right.slot_id;
			});

			bool corrected = false;
			for(size_t first_index = 0; first_index < bounds.size(); ++first_index)
			{
				const HumanColliderBounds& first_bounds = bounds[first_index];
				for(size_t second_index = first_index + 1; second_index < bounds.size(); ++second_index)
				{
					const HumanColliderBounds& second_bounds = bounds[second_index];
					if(second_bounds.minimum.x > first_bounds.maximum.x) break;
					if(first_bounds.body_id == second_bounds.body_id
						|| first_bounds.minimum.y > second_bounds.maximum.y
						|| second_bounds.minimum.y > first_bounds.maximum.y
						|| first_bounds.minimum.z > second_bounds.maximum.z
						|| second_bounds.minimum.z > first_bounds.maximum.z) continue;

					HumanCollider first;
					HumanCollider second;
					if(!getHumanCollider(world, first_bounds.slot_id, first)
						|| !getHumanCollider(world, second_bounds.slot_id, second)) continue;
					corrected |= resolveHumanPair(world, first, second);
				}
			}
			return corrected;
		}

	}

	/** @brief Separates overlapping human capsules using the UnitSystem's adaptive spatial index. */
	void PhysicsSystem::resolveHumanCollisions()
	{
		DSE_WATCH();
		DynamicArray<PointComponentSlotId> human_slot_ids;
		DynamicArray<WorldBodyId> human_body_ids;
		bool has_multiple_bodies = false;
		WorldBodyId first_human_body_id = static_cast<WorldBodyId>(-1);
		world.unit_system.collectHumanSlots(human_slot_ids, human_body_ids);
		for(size_t index = 0; index < human_slot_ids.size(); ++index)
		{
			const PointComponentSlotId slot_id = human_slot_ids[index];
			if(!world.point_data.isPointComponentSlotActive(slot_id)) continue;
			PointComponentSlot& slot = world.point_data.getPointComponentSlot(slot_id);
			if(!world.point_data.isPointActive(slot.point_id)
				|| !world.chunks.isChunkActive(slot.chunk_id)) continue;
			if(first_human_body_id == static_cast<WorldBodyId>(-1))
				first_human_body_id = human_body_ids[index];
			else has_multiple_bodies |= human_body_ids[index] != first_human_body_id;
			auto& human = *static_cast<HumanComponent*>(slot.getData());
			human.displaced_by_human_collision = false;
		}

		constexpr double broadphase_half_extent = 8.0;
		DynamicArray<PointComponentSlotId> nearby_slot_ids;
		for(uint8 iteration = 0; iteration < 4; ++iteration)
		{
			bool corrected = false;
			for(size_t first_index = 0; first_index < human_slot_ids.size(); ++first_index)
			{
				const PointComponentSlotId first_slot_id = human_slot_ids[first_index];
				HumanCollider first;
				if(!getHumanCollider(world, first_slot_id, first)) continue;
				const WorldBody& first_body = world.world_body_data.getWorldBody(first.body_id);
				const glm::dvec3 first_local_center = glm::conjugate(first_body.rotation)
					* (first.center - first_body.position);
				world.unit_system.collectNearbyHumanSlots(first.body_id, first_local_center,
					broadphase_half_extent, nearby_slot_ids);
				for(PointComponentSlotId second_slot_id : nearby_slot_ids)
				{
					if(second_slot_id <= first_slot_id) continue;
					HumanCollider first_current;
					HumanCollider second;
					if(!getHumanCollider(world, first_slot_id, first_current)
						|| !getHumanCollider(world, second_slot_id, second)
						|| first_current.body_id != second.body_id) continue;
					corrected |= resolveHumanPair(world, first_current, second);
				}

			}
			if(has_multiple_bodies)
				corrected |= resolveCrossBodyHumanPairs(world, human_slot_ids);
			if(!corrected) break;
		}

		for(PointComponentSlotId slot_id : human_slot_ids)
		{
			if(!world.point_data.isPointComponentSlotActive(slot_id)) continue;
			auto& human = *static_cast<HumanComponent*>(world.point_data.getPointComponentSlot(slot_id).getData());
			if(!human.displaced_by_human_collision) continue;
			HumanCollider collider;
			if(getHumanCollider(world, slot_id, collider)) restoreHumanGroundClearance(world, collider);
			human.displaced_by_human_collision = false;
		}
	}
}
