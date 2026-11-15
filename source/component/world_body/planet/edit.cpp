#include <danikk_space_engine/profiler.h>
#include <danikk_space_engine/component/world_body/planet/chunk_cache.h>
#include <danikk_space_engine/component/world_body/planet/planet.h>
#include <danikk_space_engine/world/face_ptr.h>
#include <danikk_space_engine/world/world.h>

namespace danikk_space_engine
{
	bool editPlanetGround(World& world, const FacePtr& face, double radius, bool adds_solid_terrain)
	{
		Watcher watcher("editPlanetGround");
		if(!world.data.isChunkActive(face.chunk_id)
			|| !world.data.isGroundTriangleActive(face.ground_triangle_id)) return false;
		Chunk& chunk = world.data.getChunk(face.chunk_id);
		if(!world.data.isWorldBodyActive(chunk.world_body_id)) return false;
		WorldBody& body = world.data.getWorldBody(chunk.world_body_id);
		const GroundTriangle& triangle = world.data.getGroundTriangle(face.ground_triangle_id);
		glm::dvec3 center = chunk.position;
		for(PointId point_id : triangle.points)
		{
			if(!world.data.isPointActive(point_id)) return false;
			center += glm::dvec3(world.data.getPoint(point_id).position) / 3.0;
		}

		for(WorldBodyComponentSlotId slot_id : body.component_slot_ids)
		{
			if(!world.data.isWorldBodyComponentSlotActive(slot_id)) continue;
			WorldBodyComponentSlot& slot = world.data.getWorldBodyComponentSlot(slot_id);
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
