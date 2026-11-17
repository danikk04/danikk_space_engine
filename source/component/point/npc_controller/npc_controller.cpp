#include <cmath>

#include <danikk_space_engine/profiler.h>
#include <danikk_space_engine/component/point/human/npc_controller.h>
#include <danikk_space_engine/component/point/human/human.h>
#include <danikk_space_engine/component/point/damageable.h>
#include <danikk_space_engine/component/point/inventory.h>
#include <danikk_space_engine/item.h>
#include <danikk_space_engine/component/world_body/planet/planet.h>
#include <danikk_space_engine/world/world.h>

#include "internal.h"

namespace danikk_space_engine
{
	using npc_internal::updateNpcChunk;
	using npc_internal::updateCombat;

	namespace
	{
		/** @brief Queues the controller's slot ID for migration after chunk callbacks finish. */
		void queueNpcChunkUpdate(World& world, const PointComponentSlot& slot)
		{
			DSE_WATCH();
			for(PointComponentSlotId id : world.data.getChunk(slot.chunk_id).point_component_slot_ids)
			{
				if(!world.data.isPointComponentSlotActive(id) || &world.data.getPointComponentSlot(id) != &slot)
					continue;
				world.pending_npc_chunk_updates.push(id);
				return;
			}
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
		DSE_WATCH();
		NpcControllerComponent& controller = *static_cast<NpcControllerComponent*>(slot.getData());
		if(!world.data.isPointActive(slot.point_id) || !world.data.isChunkActive(slot.chunk_id)) return;
		queueNpcChunkUpdate(world, slot);
		if(!world.data.isPointComponentSlotActive(controller.human_slot_id)) return;
		PointComponentSlot& human_slot = world.data.getPointComponentSlot(controller.human_slot_id);
		if(human_slot.type_id != PointComponentTypeId::human
			|| human_slot.point_id != slot.point_id || human_slot.chunk_id != slot.chunk_id)
			formatFatalError("NPC controller and human must belong to the same point and chunk");
		if(!world.data.isPointActive(slot.point_id) || !world.data.isChunkActive(slot.chunk_id)) return;
		HumanComponent& human = *static_cast<HumanComponent*>(human_slot.getData());
		human.desired_velocity = vec3(0.0f);
		const DamageableComponent* damageable = findDamageable(world, slot.chunk_id, slot.point_id);
		if(!controller.enabled || damageable == nullptr || damageable->getHealth() <= 0.0f) return;
		const float dt = glm::max(getTargetFrameDelay(), 0.0f);
		if(updateCombat(world, slot.point_id, slot.chunk_id, controller, human, dt)) return;

		npc_internal::updateNpcMovement(world, slot, controller, human, dt);
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
