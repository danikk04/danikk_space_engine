#include <danikk_space_engine/profiler.h>
#include <danikk_space_engine/component/world_body/planet/terrain_edit_save.h>
#include <danikk_space_engine/component/world_body/planet/planet.h>
#include <danikk_space_engine/world/world.h>

namespace danikk_space_engine
{

	/** @copydoc editPlanetGround(World&, WorldBodyId, const glm::dvec3&, double, bool) */
	bool editPlanetGround(World& world, WorldBodyId body_id, const glm::dvec3& position,
		double radius, bool adds_solid_terrain)
	{
		DSE_WATCH();
		if(!world.world_body_data.isWorldBodyActive(body_id) || radius <= 0.0) return false;
		WorldBody& body = world.world_body_data.getWorldBody(body_id);
		const glm::dvec3 center = glm::conjugate(body.rotation) * (position - body.position);
		for(WorldBodyComponentSlotId slot_id : body.component_slot_ids)
		{
			if(!world.world_body_data.isWorldBodyComponentSlotActive(slot_id)) continue;
			WorldBodyComponentSlot& slot = world.world_body_data.getWorldBodyComponentSlot(slot_id);
			if(slot.type_id != WorldBodyComponentTypeId::planet) continue;
			PlanetComponent& planet = *static_cast<PlanetComponent*>(slot.getData());
			planet.terrain_edits.pushMove(PlanetTerrainEdit{center, radius, adds_solid_terrain});
			planet.terrain_revision++;
			if(planet.terrain_revision == 0) planet.terrain_revision++;
			savePlanetTerrainEdits(
				slot.world_body_id,
				planet.radius,
				planet.noise_seed,
				planet.terrain_edits,
				planet.terrain_revision);
			return true;
		}
		return false;
	}
}
