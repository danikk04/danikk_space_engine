#include <cmath>

#include <danikk_space_engine/profiler.h>
#include <danikk_space_engine/world_physics.h>
#include <danikk_space_engine/component/point/human/human.h>
#include <danikk_space_engine/world/world.h>

namespace danikk_space_engine
{
	namespace
	{
		constexpr double minimum_ground_normal_dot = 0.2;

		/**
		 * @brief Represents human collider data and behavior.
		 */
		struct HumanCollider
		{
			PointId point_id;
			ChunkId chunk_id;
			glm::dvec3 center;
			glm::dvec3 up;
			double height;
			double radius;
		};

		// Finds closest points on two standing capsule axes, including degenerate endpoint cases.

		/**
		 * @brief Performs the closest segment points operation.
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

		// Applies a world-space correction to the human center through its owning body's local frame.

		/**
		 * @brief Moves human.
		 */
		void moveHuman(World& world, HumanCollider& human, const glm::dvec3& world_delta)
		{
			const Chunk& chunk = world.data.getChunk(human.chunk_id);
			const WorldBody& body = world.data.getWorldBody(chunk.world_body_id);
			Point& point = world.data.getPoint(human.point_id);
			point.position += vec3(glm::conjugate(body.rotation) * world_delta);
			human.center += world_delta;
		}

		// Tangential separation can push a capsule into uneven terrain. Probe below its feet
		// afterward and lift it back to the supporting surface, never snapping it farther down.

		/**
		 * @brief Performs the restore human ground clearance operation.
		 */
		void restoreHumanGroundClearance(World& world, HumanCollider& human)
		{
			const glm::dvec3 foot_position = human.center - human.up * (human.height * 0.5);
			constexpr double probe_height = 1.0;
			constexpr double maximum_lift = 1.0;
			const RaycastHit hit = world.raycast(
				foot_position + human.up * probe_height,
				vec3(-human.up),
				static_cast<float>(probe_height + maximum_lift));
			if(!hit.hit || !world.data.isChunkActive(hit.face.chunk_id)) return;
			const Chunk& hit_chunk = world.data.getChunk(hit.face.chunk_id);
			const Chunk& human_chunk = world.data.getChunk(human.chunk_id);
			if(hit_chunk.world_body_id != human_chunk.world_body_id
				|| glm::dot(glm::dvec3(hit.normal), human.up) < minimum_ground_normal_dot) return;

			const glm::dvec3 supported_center = hit.position + human.up * (human.height * 0.5 + 0.02);
			const glm::dvec3 correction = supported_center - human.center;
			const double lift = glm::dot(correction, human.up);
			if(lift <= 0.0001 || lift > maximum_lift) return;
			moveHuman(world, human, correction);
		}
	}

	// Collects humans as vertical capsules, performs pairwise separation sweeps, then restores
	// ground clearance for characters whose tangential correction crossed uneven terrain.

	/**
	 * @brief Resolves human collisions.
	 */
	void WorldPhysics::resolveHumanCollisions(World& world)
	{
		/**
		 * @brief Disables copying to ensure a measured scope is recorded once.
		 */
		Watcher watcher("World::human collisions");
		DynamicArray<HumanCollider> humans;

		for(PointComponentSlotId slot_id : world.data.getUsedPointComponentSlotIds())
		{
			if(!world.data.isPointComponentSlotActive(slot_id)) continue;
			const PointComponentSlot& slot = world.data.getPointComponentSlot(slot_id);
			if(slot.type_id != PointComponentTypeId::human || !world.data.isChunkActive(slot.chunk_id)
				|| !world.data.isPointActive(slot.point_id)) continue;
			const HumanComponent& human = *static_cast<const HumanComponent*>(slot.getData());
			if(human.height <= 0.0f) continue;
			const Chunk& chunk = world.data.getChunk(slot.chunk_id);
			const WorldBody& body = world.data.getWorldBody(chunk.world_body_id);
			const glm::dvec3 up = glm::normalize(body.rotation * glm::dvec3(glm::normalize(human.up)));
			const double height = human.height;
			humans.push(HumanCollider{
				slot.point_id,
				slot.chunk_id,
				world.getPointWorldPosition(slot.chunk_id, slot.point_id),
				up,
				height,
				height * 0.14});
		}

		for(uint8 iteration = 0; iteration < 4; iteration++)
		{
			for(size_t a_index = 0; a_index < humans.size(); a_index++)
			{
				HumanCollider& a = humans[a_index];
				const glm::dvec3 a0 = a.center - a.up * (a.height * 0.5 - a.radius);
				const glm::dvec3 a1 = a.center + a.up * (a.height * 0.5 - a.radius);
				for(size_t b_index = a_index + 1; b_index < humans.size(); b_index++)
				{
					HumanCollider& b = humans[b_index];
					const double maximum_distance = (a.height + b.height) * 0.5 + a.radius + b.radius;
					const glm::dvec3 center_delta = b.center - a.center;
					if(glm::dot(center_delta, center_delta) > maximum_distance * maximum_distance) continue;
					const glm::dvec3 b0 = b.center - b.up * (b.height * 0.5 - b.radius);
					const glm::dvec3 b1 = b.center + b.up * (b.height * 0.5 - b.radius);
					glm::dvec3 closest_a;
					glm::dvec3 closest_b;
					closestSegmentPoints(a0, a1, b0, b1, closest_a, closest_b);
					glm::dvec3 normal = closest_b - closest_a;
					const double distance = glm::length(normal);
					const double penetration = a.radius + b.radius - distance;
					if(penetration <= 0.0) continue;

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
					if(total_factor <= 1e-9) continue;
					const double correction_distance = penetration / total_factor;
					if(move_a_factor > 1e-9)
						moveHuman(world, a, glm::normalize(move_a) * correction_distance);
					if(move_b_factor > 1e-9)
						moveHuman(world, b, glm::normalize(move_b) * correction_distance);
				}
			}
		}

		for(HumanCollider& human : humans)
		{
			restoreHumanGroundClearance(world, human);
		}
	}
}
