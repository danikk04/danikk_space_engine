#include <cmath>

#include <danikk_space_engine/profiler.h>
#include <danikk_space_engine/component/point/human/npc_controller.h>
#include <danikk_space_engine/component/point/human/human.h>
#include <danikk_space_engine/component/point/damageable.h>
#include <danikk_space_engine/component/point/inventory.h>
#include <danikk_space_engine/item.h>
#include <danikk_space_engine/component/world_body/planet/planet.h>
#include <danikk_space_engine/world/world.h>

namespace danikk_space_engine
{
	namespace
	{
		constexpr bool log_npc_chunk_transitions = false;
		/** @brief Moves an NPC and its point components to the loaded planet chunk containing its position. */
		void updateNpcChunk(World& world, PointId point_id, ChunkId old_chunk_id, NpcControllerComponent& controller)
		{
			Chunk& old_chunk = world.data.getChunk(old_chunk_id);
			WorldBody& body = world.data.getWorldBody(old_chunk.world_body_id);
			const glm::dvec3 position = old_chunk.position + glm::dvec3(world.data.getPoint(point_id).position);
			const glm::ivec3 coordinate = glm::ivec3(glm::floor(position / planet_chunk_width));
			ChunkId new_chunk_id = static_cast<ChunkId>(-1);
			bool is_planet = false;
			for(WorldBodyComponentSlotId id : body.component_slot_ids)
			{
				if(!world.data.isWorldBodyComponentSlotActive(id)) continue;
				WorldBodyComponentSlot& component = world.data.getWorldBodyComponentSlot(id);
				if(component.type_id != WorldBodyComponentTypeId::planet) continue;
				is_planet = true;
				PlanetComponent& planet = *static_cast<PlanetComponent*>(component.getData());
				planet.chunk_lookup.getChunk(coordinate, new_chunk_id);
				break;
			}
			if(!is_planet || new_chunk_id == old_chunk_id) return;
			if(!world.data.isChunkActive(new_chunk_id))
			{
				const glm::dvec3 minimum = glm::dvec3(old_chunk.grid_coordinate) * planet_chunk_width;
				const glm::dvec3 maximum = minimum + glm::dvec3(planet_chunk_width - 0.001);
				world.data.getPoint(point_id).position = vec3(glm::clamp(position, minimum, maximum) - old_chunk.position);
				return;
			}
			const glm::dvec3 new_origin = world.data.getChunk(new_chunk_id).position;
			DynamicArray<PointComponentSlotId> moved;
			for(size_t i = 0; i < old_chunk.point_component_slot_ids.size();)
			{
				const PointComponentSlotId id = old_chunk.point_component_slot_ids[i];
				if(world.data.getPointComponentSlot(id).point_id != point_id) { ++i; continue; }
				moved.push(id);
				for(size_t j = i; j + 1 < old_chunk.point_component_slot_ids.size(); ++j)
					old_chunk.point_component_slot_ids[j] = old_chunk.point_component_slot_ids[j + 1];
				old_chunk.point_component_slot_ids.resize(old_chunk.point_component_slot_ids.size() - 1);
			}
			for(size_t i = 0; i < old_chunk.point_ids.size(); ++i)
			{
				if(old_chunk.point_ids[i] != point_id) continue;
				for(size_t j = i; j + 1 < old_chunk.point_ids.size(); ++j) old_chunk.point_ids[j] = old_chunk.point_ids[j + 1];
				old_chunk.point_ids.resize(old_chunk.point_ids.size() - 1);
				world.data.getPoint(point_id).referencing_chunk_count--;
				break;
			}
			if(controller.has_movement_target)
				controller.target_position += vec3(old_chunk.position - new_origin);
			world.data.getPoint(point_id).position = vec3(position - new_origin);
			for(PointComponentSlotId id : moved) world.data.addPointComponentSlotToChunk(new_chunk_id, id);
			if constexpr(log_npc_chunk_transitions)
			{
				const glm::ivec3 old_coordinate = old_chunk.grid_coordinate;
				formatLogInfo("NPC point % moved from chunk % (%, %, %) to chunk % (%, %, %)",
					point_id, old_chunk_id, old_coordinate.x, old_coordinate.y, old_coordinate.z,
					new_chunk_id, coordinate.x, coordinate.y, coordinate.z);
			}
		}

		/** @brief Checks that a candidate still represents a living hostile human. */
		bool validEnemy(World& world, const NpcTarget& target, FactionId faction)
		{
			if(!world.data.isPointComponentSlotActive(target.slot_id)) return false;
			const PointComponentSlot& slot = world.data.getPointComponentSlot(target.slot_id);
			if(slot.type_id != PointComponentTypeId::human || slot.point_id != target.point_id || slot.generation_id != target.generation_id
				|| !world.data.isPointActive(slot.point_id) || !world.data.isChunkActive(slot.chunk_id)) return false;
			const HumanComponent& human = *static_cast<const HumanComponent*>(slot.getData());
			const DamageableComponent* health = findDamageable(world, slot.chunk_id, slot.point_id);
			return health != nullptr && health->health > 0.0f && faction < world.data.factions.size()
				&& human.faction_id < world.data.factions.size()
				&& world.data.factions.getRelation(faction, human.faction_id).type == FactionRelationType::hostile;
		}

		/** @brief Collects hostile people from the loaded planet chunks around the NPC. */
		void collectCandidates(World& world, PointId point_id, ChunkId chunk_id, FactionId faction,
			NpcControllerComponent& controller)
		{
			controller.enemy_candidates.clear();
			controller.candidate_index = 0;
			const Chunk& owner = world.data.getChunk(chunk_id);
			const WorldBody& body = world.data.getWorldBody(owner.world_body_id);
			const glm::ivec3 coordinate = glm::ivec3(glm::floor(
				(owner.position + glm::dvec3(world.data.getPoint(point_id).position)) / planet_chunk_width));
			const PlanetComponent* planet = nullptr;
			for(WorldBodyComponentSlotId id : body.component_slot_ids)
			{
				if(!world.data.isWorldBodyComponentSlotActive(id)) continue;
				const WorldBodyComponentSlot& slot = world.data.getWorldBodyComponentSlot(id);
				if(slot.type_id == WorldBodyComponentTypeId::planet)
				{
					planet = static_cast<const PlanetComponent*>(slot.getData());
					break;
				}
			}
			auto collect = [&](ChunkId candidate_chunk)
			{
				if(!world.data.isChunkActive(candidate_chunk)) return;
				for(PointComponentSlotId id : world.data.getChunk(candidate_chunk).point_component_slot_ids)
				{
					if(!world.data.isPointComponentSlotActive(id)) continue;
					const PointComponentSlot& slot = world.data.getPointComponentSlot(id);
					if(slot.type_id != PointComponentTypeId::human || slot.point_id == point_id) continue;
					const NpcTarget target{id, slot.point_id, slot.generation_id, 0.0f};
					if(validEnemy(world, target, faction)) controller.enemy_candidates.push(target);
				}
			};
			if(planet == nullptr) { collect(chunk_id); return; }
			for(int x = -1; x <= 1; ++x)
				for(int y = -1; y <= 1; ++y)
					for(int z = -1; z <= 1; ++z)
					{
						ChunkId nearby = static_cast<ChunkId>(-1);
						if(planet->chunk_lookup.getChunk(coordinate + glm::ivec3(x, y, z), nearby)) collect(nearby);
					}
		}

		/** @brief Checks one candidate per tick, rotates targets each second and fires at the active target. */
		bool updateCombat(World& world, PointId point_id, ChunkId chunk_id,
			NpcControllerComponent& controller, HumanComponent& human, float dt)
		{
			const Chunk& own_chunk = world.data.getChunk(chunk_id);
			const glm::ivec3 own_coordinate = glm::ivec3(glm::floor(
				(own_chunk.position + glm::dvec3(world.data.getPoint(point_id).position)) / planet_chunk_width));
			auto nearbyEnemy = [&](const NpcTarget& target)
			{
				if(!validEnemy(world, target, human.faction_id)) return false;
				const PointComponentSlot& other = world.data.getPointComponentSlot(target.slot_id);
				const Chunk& other_chunk = world.data.getChunk(other.chunk_id);
				if(other_chunk.world_body_id != own_chunk.world_body_id) return false;
				const glm::ivec3 other_coordinate = glm::ivec3(glm::floor(
					(other_chunk.position + glm::dvec3(world.data.getPoint(other.point_id).position)) / planet_chunk_width));
				const glm::ivec3 difference = glm::abs(other_coordinate - own_coordinate);
				return difference.x <= 1 && difference.y <= 1 && difference.z <= 1;
			};
			for(NpcTarget& target : controller.targeted_enemy)
			{
				target.visibility_age += dt;
				if(!nearbyEnemy(target) || target.visibility_age > 2.0f) target = NpcTarget{};
			}
			if(controller.candidate_index >= controller.enemy_candidates.size())
				collectCandidates(world, point_id, chunk_id, human.faction_id, controller);

			const Chunk& chunk = world.data.getChunk(chunk_id);
			const glm::dquat rotation = world.data.getWorldBody(chunk.world_body_id).rotation;
			const glm::dvec3 origin = world.getPointWorldPosition(chunk_id, point_id);
			const glm::dvec3 shot_origin = origin + rotation * glm::dvec3(human.up * (human.height * 0.2f));
			if(controller.candidate_index < controller.enemy_candidates.size())
			{
				const NpcTarget candidate = controller.enemy_candidates[controller.candidate_index++];
				if(nearbyEnemy(candidate))
				{
					const PointComponentSlot& target_slot = world.data.getPointComponentSlot(candidate.slot_id);
					const glm::dvec3 delta = world.getPointWorldPosition(target_slot.chunk_id, candidate.point_id) - shot_origin;
					const double distance = glm::length(delta);
					bool visible = false;
					if(distance > 0.001)
					{
						const RaycastHit hit = world.raycast(shot_origin, vec3(delta), static_cast<float>(distance));
						visible = !hit.hit || hit.distance >= distance - 0.1;
					}
					int existing = -1;
					int free_index = -1;
					for(int i = 0; i < 3; ++i)
					{
						if(controller.targeted_enemy[i].slot_id == candidate.slot_id && controller.targeted_enemy[i].point_id == candidate.point_id) existing = i;
						if(controller.targeted_enemy[i].slot_id == static_cast<PointComponentSlotId>(-1) && free_index < 0) free_index = i;
					}
					if(!visible && existing >= 0) controller.targeted_enemy[existing] = NpcTarget{};
					if(visible && existing >= 0) controller.targeted_enemy[existing].visibility_age = 0.0f;
					if(visible && existing < 0)
					{
						if(free_index < 0)
						{
							double farthest = glm::dot(delta, delta);
							for(int i = 0; i < 3; ++i)
							{
								const NpcTarget& current = controller.targeted_enemy[i];
								const PointComponentSlot& current_slot = world.data.getPointComponentSlot(current.slot_id);
								const glm::dvec3 offset = world.getPointWorldPosition(current_slot.chunk_id, current.point_id) - shot_origin;
								const double distance_squared = glm::dot(offset, offset);
								if(distance_squared > farthest) { farthest = distance_squared; free_index = i; }
							}
						}
						if(free_index >= 0) controller.targeted_enemy[free_index] = candidate;
					}
				}
			}
			controller.target_switch_timer += dt;
			while(controller.target_switch_timer >= 1.0f)
			{
				controller.target_switch_timer -= 1.0f;
				for(int i = 0; i < 3; ++i)
				{
					controller.target_index = (controller.target_index + 1) % 3;
					if(controller.targeted_enemy[controller.target_index].slot_id != static_cast<PointComponentSlotId>(-1)) break;
				}
			}
			if(controller.targeted_enemy[controller.target_index].slot_id == static_cast<PointComponentSlotId>(-1))
				for(uint8 i = 0; i < 3; ++i)
					if(controller.targeted_enemy[i].slot_id != static_cast<PointComponentSlotId>(-1)) { controller.target_index = i; break; }
			const NpcTarget target = controller.targeted_enemy[controller.target_index];
			if(!nearbyEnemy(target)) return false;
			const PointComponentSlot& target_slot = world.data.getPointComponentSlot(target.slot_id);
			const glm::dvec3 target_position = world.getPointWorldPosition(target_slot.chunk_id, target.point_id);
			const vec3 direction = vec3(glm::conjugate(rotation) * (target_position - shot_origin));
			const float vertical = glm::dot(direction, human.up);
			const vec3 horizontal = direction - human.up * vertical;
			const float length = glm::length(horizontal);
			if(length < 0.001f) return false;
			human.forward = horizontal / length;
			human.head_rotation = vec2(0.0f, glm::clamp(std::atan2(vertical, length), -0.8f, 0.8f));
			human.desired_velocity = vec3(0.0f);
			for(PointComponentSlotId id : world.data.getChunk(chunk_id).point_component_slot_ids)
			{
				PointComponentSlot& component = world.data.getPointComponentSlot(id);
				if(component.point_id != point_id || component.type_id != PointComponentTypeId::inventory) continue;
				InventoryComponent& inventory = *static_cast<InventoryComponent*>(component.getData());
				for(InventorySlot& item : inventory.slots)
				{
					if(item.isEmpty() || !item.is_equipped || getItemDefinition(item.item_definition_id).type_id != item_type_ids::weapon) continue;
					useItem(world, point_id, item);
					return true;
				}
			}
			return true;
		}

	}

	/**
	 * @brief Creates default NPC state or copies optional behavior state into slot storage.
	 * @param destination Destination storage owned by the point-component slot.
	 * @param init_data Optional source `NpcControllerComponent`; null selects member defaults.
	 */
	void constructNpcControllerComponent(void* destination, const void* init_data)
	{
		if(destination == nullptr) formatFatalError("NPC controller construction received null destination");
		new (destination) NpcControllerComponent(init_data == nullptr
			? NpcControllerComponent() : *static_cast<const NpcControllerComponent*>(init_data));
	}

	/**
	 * @brief Copies NPC behavior state into relocated storage and ends the source lifetime.
	 * @param destination Replacement payload storage.
	 * @param source Existing live NPC controller payload.
	 */
	void moveNpcControllerComponent(void* destination, void* source)
	{
		if(destination == nullptr || source == nullptr) formatFatalError("NPC controller move received null memory");
		new (destination) NpcControllerComponent(*static_cast<NpcControllerComponent*>(source));
		static_cast<NpcControllerComponent*>(source)->~NpcControllerComponent();
	}

	/**
	 * @brief Ends the NPC controller payload lifetime; WorldData releases its slot storage.
	 * @param data Existing live controller payload.
	 */
	void destroyNpcControllerComponent(void* data)
	{
		if(data == nullptr) formatFatalError("NPC controller destruction received null data");
		static_cast<NpcControllerComponent*>(data)->~NpcControllerComponent();
	}

	/**
	 * @brief Validates the paired human and writes the next movement and head-look command.
	 *
	 * Explicit movement targets and visible hostile targets drive commands; the HumanComponent
	 * later applies movement and handles physics.
	 * @param slot NPC controller slot, expected to share point and chunk IDs with its human.
	 * @param world World used for component validation and optional camera tracking.
	 */
	void tickNpcControllerComponent(PointComponentSlot& slot, World& world)
	{
		/**
		 * @brief Measures the duration of the enclosing profiler scope.
		 */
		Watcher watcher("NpcControllerComponent::tick");
		NpcControllerComponent& controller = *static_cast<NpcControllerComponent*>(slot.getData());
		if(!world.data.isPointActive(slot.point_id) || !world.data.isChunkActive(slot.chunk_id)) return;
		for(PointComponentSlotId id : world.data.getChunk(slot.chunk_id).point_component_slot_ids)
			if(world.data.isPointComponentSlotActive(id) && &world.data.getPointComponentSlot(id) == &slot)
			{
				world.pending_npc_chunk_updates.push(id);
				break;
			}
		if(!world.data.isPointComponentSlotActive(controller.human_slot_id)) return;
		PointComponentSlot& human_slot = world.data.getPointComponentSlot(controller.human_slot_id);
		if(human_slot.type_id != PointComponentTypeId::human
			|| human_slot.point_id != slot.point_id || human_slot.chunk_id != slot.chunk_id)
			formatFatalError("NPC controller and human must belong to the same point and chunk");
		if(!world.data.isPointActive(slot.point_id) || !world.data.isChunkActive(slot.chunk_id)) return;
		HumanComponent& human = *static_cast<HumanComponent*>(human_slot.getData());
		human.desired_velocity = vec3(0.0f);
		const DamageableComponent* damageable = findDamageable(world, slot.chunk_id, slot.point_id);
		if(!controller.enabled || damageable == nullptr || damageable->health <= 0.0f) return;
		const float dt = glm::max(getTargetFrameDelay(), 0.0f);
		if(updateCombat(world, slot.point_id, slot.chunk_id, controller, human, dt)) return;

		if(controller.has_movement_target)
		{
			vec3 offset = controller.target_position - world.data.getPoint(slot.point_id).position;
			offset -= human.up * glm::dot(offset, human.up);
			const float distance = glm::length(offset);
			if(distance <= 0.02f)
			{
				controller.has_movement_target = false;
			}
			else if(dt > 0.0f)
				human.desired_velocity = offset / distance
					* glm::min(glm::max(human.movement_speed, 0.0f), distance / dt);
		}

		if(controller.look_at_camera && world.data.isPointComponentSlotActive(world.camera_component_slot_id))
		{
			const Chunk& chunk = world.data.getChunk(slot.chunk_id);
			const WorldBody& body = world.data.getWorldBody(chunk.world_body_id);
			const glm::dvec3 head_position = world.getPointWorldPosition(slot.chunk_id, slot.point_id)
				+ body.rotation * glm::dvec3(human.up * (human.height * 0.375f));
			const vec3 direction = vec3(glm::conjugate(body.rotation)
				* (world.getCameraWorldPosition() - head_position));
			const vec3 right = glm::normalize(glm::cross(human.forward, human.up));
			const float front = glm::dot(direction, human.forward);
			const float side = glm::dot(direction, right);
			const float vertical = glm::dot(direction, human.up);
			human.head_rotation.x = glm::clamp(std::atan2(-side, front), -1.2f, 1.2f);
			human.head_rotation.y = glm::clamp(std::atan2(vertical, std::sqrt(front * front + side * side)), -0.8f, 0.8f);
		}
	}

	/** @copydoc updateNpcChunkAfterTick */
	void updateNpcChunkAfterTick(World& world, PointComponentSlotId slot_id)
	{
		if(!world.data.isPointComponentSlotActive(slot_id)) return;
		PointComponentSlot& slot = world.data.getPointComponentSlot(slot_id);
		if(slot.type_id != PointComponentTypeId::npc_controller || !world.data.isPointActive(slot.point_id)
			|| !world.data.isChunkActive(slot.chunk_id)) return;
		updateNpcChunk(world, slot.point_id, slot.chunk_id, *static_cast<NpcControllerComponent*>(slot.getData()));
	}
}
