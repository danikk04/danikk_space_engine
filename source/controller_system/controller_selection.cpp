#include <danikk_space_engine/controller_system.h>
#include <danikk_space_engine/component/point/human/human.h>
#include <danikk_space_engine/factions.h>
#include <danikk_space_engine/math/ray.h>
#include <danikk_space_engine/world/world.h>

namespace danikk_space_engine
{
	/** @brief Removes stale, dead, or no-longer-player-owned selection references. */
	void ControllerSystem::validateSelectedUnits()
	{
		size_t write_index = 0;
		for(size_t read_index = 0; read_index < selected_unit_refs.size(); ++read_index)
		{
			const SelectedUnitRef reference = selected_unit_refs[read_index];
			if(!world.point_data.isPointComponentSlotActive(reference.slot_id)) continue;
			const PointComponentSlot& slot = world.point_data.getPointComponentSlot(reference.slot_id);
			if(slot.generation_id != reference.generation_id
				|| slot.type_id != PointComponentTypeId::human
				|| !world.point_data.isPointActive(slot.point_id)
				|| !world.chunks.isChunkActive(slot.chunk_id)) continue;
			const HumanComponent& human = *static_cast<const HumanComponent*>(slot.getData());
			if(human.faction_id != base_faction_ids::player || human.hasDeathTimestamp()) continue;
			selected_unit_refs[write_index++] = reference;
		}
		selected_unit_refs.resize(write_index);
	}

	/**
	 * @brief Selects the nearest visible player-faction human intersected by the camera ray.
	 * @param surface_hit Nearest terrain or BuildPart hit used to reject units behind solid geometry.
	 * @param shift_pressed Keeps the current group and toggles the hit unit; otherwise replaces it.
	 * @return True if the click selected or deselected a human.
	 */
	bool ControllerSystem::selectOwnHumanUnderCursor(
		const RaycastHit& surface_hit, bool shift_pressed)
	{
		const glm::dvec3 ray_origin = world.getCameraWorldPosition();
		const glm::dvec3 ray_direction = glm::normalize(glm::dvec3(world.getCameraComponent().getFront()));
		double closest_distance = surface_hit.hit ? surface_hit.distance : 10000.0;
		PointComponentSlotId closest_slot_id = static_cast<PointComponentSlotId>(-1);

		for(PointComponentSlotId slot_id : world.point_data.getUsedPointComponentSlotIds())
		{
			if(!world.point_data.isPointComponentSlotActive(slot_id)) continue;
			const PointComponentSlot& slot = world.point_data.getPointComponentSlot(slot_id);
			if(slot.type_id != PointComponentTypeId::human
				|| !world.point_data.isPointActive(slot.point_id)
				|| !world.chunks.isChunkActive(slot.chunk_id)) continue;
			const HumanComponent& human = *static_cast<const HumanComponent*>(slot.getData());
			if(human.faction_id != base_faction_ids::player || human.hasDeathTimestamp()) continue;

			const Chunk& chunk = world.chunks.getChunk(slot.chunk_id);
			const WorldBody& body = world.world_body_data.getWorldBody(chunk.world_body_id);
			const glm::dvec3 center = world.getPointWorldPosition(slot.chunk_id, slot.point_id);
			glm::dvec3 up = body.rotation * glm::dvec3(human.up);
			const double up_length = glm::length(up);
			if(up_length <= 1e-9) up = glm::dvec3(0.0, 0.0, 1.0);
			else up /= up_length;
			double distance = 0.0;
			const double height = human.getHeight();
			if(intersectRayCapsule(ray_origin, ray_direction, center, up,
				height, height * 0.14, distance) && distance < closest_distance)
			{
				closest_distance = distance;
				closest_slot_id = slot_id;
			}
		}

		if(!shift_pressed) selected_unit_refs.clear();
		if(closest_slot_id == static_cast<PointComponentSlotId>(-1)) return false;
		const PointComponentSlot& slot = world.point_data.getPointComponentSlot(closest_slot_id);
		for(size_t index = 0; index < selected_unit_refs.size(); ++index)
		{
			if(selected_unit_refs[index].slot_id != closest_slot_id
				|| selected_unit_refs[index].generation_id != slot.generation_id) continue;
			if(shift_pressed)
			{
				for(size_t move_index = index + 1; move_index < selected_unit_refs.size(); ++move_index)
					selected_unit_refs[move_index - 1] = selected_unit_refs[move_index];
				selected_unit_refs.resize(selected_unit_refs.size() - 1);
			}
			return true;
		}
		selected_unit_refs.pushCtor(SelectedUnitRef{closest_slot_id, slot.generation_id});
		return true;
	}
}
