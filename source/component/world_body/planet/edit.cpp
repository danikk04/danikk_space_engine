#include <danikk_space_engine/profiler.h>
#include <danikk_space_engine/component/world_body/planet/chunk_save.h>
#include <danikk_space_engine/component/world_body/planet/planet.h>
#include <danikk_space_engine/world/face_ptr.h>
#include <danikk_space_engine/world/world.h>

namespace danikk_space_engine
{

	/**
	 * @brief Converts a selected triangle to a planet-local sphere stamp and persists the edit.
	 * @param world World containing the selected face and owning planet component.
	 * @param face Chunk and triangle IDs identifying the edit center.
	 * @param radius Radius of the additive or subtractive sphere in world units.
	 * @param adds_solid_terrain True to add solid terrain; false to remove it.
	 * @return True when the face and owning planet are valid and the edit was appended.
	 * The center is the triangle centroid in planet-body coordinates. The edit
	 * revision is advanced (skipping zero) and the full ordered stamp list is
	 * saved; the next planet update invalidates intersecting chunks for rebuild.
	 */
	bool editPlanetGround(World& world, const FacePtr& face, double radius, bool adds_solid_terrain)
	{
		/**
		 * @brief Measures the duration of the enclosing profiler scope.
		 */
		DSE_WATCH();
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
