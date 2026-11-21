#include <danikk_space_engine/systems/controller/controller.h>
#include <danikk_space_engine/systems/human/human.h>
#include <danikk_space_engine/systems/faction/faction.h>
#include <danikk_space_engine/math/ray.h>
#include <danikk_space_engine/world.h>

namespace danikk_space_engine
{
	/** @brief Removes stale, dead, or no-longer-player-owned selection references. */
	void ControllerSystem::validateSelectedUnits()
	{
		size_t write_index = 0;
		for(size_t read_index = 0; read_index < selected_unit_refs.size(); ++read_index)
		{
			const SelectedUnitRef reference = selected_unit_refs[read_index];
			const HumanData* human_data = world.humans.findHuman(reference.human_id);
			if(human_data == nullptr || human_data->generation_id != reference.generation_id
				|| !world.world_body_data.isWorldBodyActive(human_data->world_body_id)) continue;
			const HumanState& human = *human_data;
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
		const glm::dvec3 ray_origin = world.camera.getWorldPosition();
		const glm::dvec3 ray_direction = glm::normalize(glm::dvec3(world.camera.getFront()));
		double closest_distance = surface_hit.hit ? surface_hit.distance : 10000.0;
		HumanId closest_slot_id = static_cast<HumanId>(-1);

		for(const HumanData& data : world.humans.getHumans())
		{
			if(!world.world_body_data.isWorldBodyActive(data.world_body_id)) continue;
			const HumanState& human = data;
			if(human.faction_id != base_faction_ids::player || human.hasDeathTimestamp()) continue;

			const WorldBody& body = world.world_body_data.getWorldBody(data.world_body_id);
			const glm::dvec3 center = world.humans.getWorldPosition(data);
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
				closest_slot_id = data.id;
			}
		}

		if(!shift_pressed) selected_unit_refs.clear();
		if(closest_slot_id == static_cast<HumanId>(-1)) return false;
		const HumanData* data = world.humans.findHuman(closest_slot_id);
		if(data == nullptr) return false;
		for(size_t index = 0; index < selected_unit_refs.size(); ++index)
		{
			if(selected_unit_refs[index].human_id != closest_slot_id
				|| selected_unit_refs[index].generation_id != data->generation_id) continue;
			if(shift_pressed)
			{
				for(size_t move_index = index + 1; move_index < selected_unit_refs.size(); ++move_index)
					selected_unit_refs[move_index - 1] = selected_unit_refs[move_index];
				selected_unit_refs.resize(selected_unit_refs.size() - 1);
			}
			return true;
		}
		selected_unit_refs.pushCtor(SelectedUnitRef{closest_slot_id, data->generation_id});
		return true;
	}
}
