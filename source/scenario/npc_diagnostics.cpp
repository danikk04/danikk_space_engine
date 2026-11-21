#include <cmath>
#include <cstdlib>
#include <limits>

#include <danikk_framework/log.h>
#include <danikk_space_engine/material.h>
#include <danikk_space_engine/profiler.h>
#include <danikk_space_engine/scenario/npc_diagnostics.h>
#include <danikk_space_engine/component/point/human/human.h>
#include <danikk_space_engine/systems/planet/system.h>
#include <danikk_space_engine/systems/unit/unit.h>
#include <danikk_space_engine/world.h>

namespace danikk_space_engine
{
	namespace
	{
		constexpr size_t npc_columns = 20;
		constexpr size_t npc_rows = 10;
		constexpr size_t npc_count = npc_columns * npc_rows;
		constexpr double spacing = 2.4;
		constexpr double hill_radius = 96.0;

		/** @brief Tracks a diagnostic NPC and its center at the start of each phase. */
		struct DiagnosticNpc
		{
			PointComponentSlotId slot_id;
			glm::dvec3 start_position;
		};

		/** @brief Counts survivors, grounded characters and net movement at a phase boundary. */
		struct PhaseResult
		{
			size_t active = 0;
			size_t grounded = 0;
			size_t moved = 0;
		};

		/** @brief Reads the point center in planet coordinates without adding profiler scopes. */
		glm::dvec3 getNpcPosition(const World& world, const PointComponentSlot& slot)
		{
			return world.getPointWorldPosition(slot.point_id);
		}

		/**
		 * @brief Attaches a human and health component, then registers a disabled central agent.
		 * Movement comes from the scenario so faction combat and input cannot change the test.
		 */
		PointComponentSlotId spawnDiagnosticNpc(World& world, WorldBodyId body_id, FactionId faction_id,
			const glm::dvec3& feet)
		{
			PointData& data = world.point_data;
			const PointId point_id = data.createPoint();
			HumanComponent human;
			human.faction_id = faction_id;
			human.up = vec3(glm::normalize(feet));
			const glm::dvec3 center = feet + glm::dvec3(human.up) * (human.getHeight() * 0.5);
			data.getPoint(point_id).body_local_position = center;

			const PointComponentSlotId human_id = data.createPointComponentSlot(point_id, PointComponentTypeId::human, &human);
			world.attachPointComponentToBody(body_id, point_id, human_id, center);
			const PointComponentSlotId health_id = data.createPointComponentSlot(point_id, PointComponentTypeId::damageable);
			world.attachPointComponentToBody(body_id, point_id, health_id, center);

			world.unit_system.registerAgent(human_id, false);
			return human_id;
		}

		/**
		 * @brief Creates a small loaded region around an in-memory hill and spawns a grid of NPCs.
		 * The hill sits above the surrounding procedural terrain so cave placement and player saves
		 * cannot affect the initial support. Actors are attached directly to the body's octree.
		 */
		void createNpcFixture(World& world, WorldBodyId body_id, PlanetData& planet,
			DiagnosticNpc (&npcs)[npc_count])
		{
			const PlanetSettings& planet_settings = world.planets.getPlanetSettings(planet.settings_id);
			const glm::dvec3 hill_center(0.0, 0.0, planet_settings.radius + 64.0 - hill_radius);
			planet.terrain_edits.append(
				PlanetTerrainEdit{hill_center, hill_radius, default_materials::stone});
			++planet.terrain_revision;
			for(size_t i = 0; i < npc_count; ++i)
			{
				const double x = (static_cast<double>(i % npc_columns) - (npc_columns - 1) * 0.5) * spacing;
				const double y = (static_cast<double>(i / npc_columns) - (npc_rows - 1) * 0.5) * spacing;
				const double z = hill_center.z + std::sqrt(hill_radius * hill_radius - x * x - y * y);
				const glm::dvec3 feet(x, y, z + 0.03);
				const FactionId faction = i % 2 == 0 ? base_faction_ids::player : base_faction_ids::pirates;
				npcs[i].slot_id = spawnDiagnosticNpc(world, body_id, faction, feet);
			}
		}

		/**
		 * @brief Measures a fixed number of simulation ticks and checks the actors at the phase boundary.
		 * Scripts the same velocity for each live NPC before the measured tick starts. Forces a
		 * profiler report after the final tick so short or fast phases are always logged.
		 */
		PhaseResult measureNpcPhase(World& world, DiagnosticNpc (&npcs)[npc_count], const char* name,
			uint32 ticks, const vec3& velocity)
		{
			formatLogInfo("NPC diagnostics phase=% ticks=%", name, ticks);
			for(DiagnosticNpc& npc : npcs)
				if(world.point_data.isPointComponentSlotActive(npc.slot_id))
				npc.start_position = getNpcPosition(world, world.point_data.getPointComponentSlot(npc.slot_id));

			for(uint32 tick = 0; tick < ticks; ++tick)
			{
				for(const DiagnosticNpc& npc : npcs)
				{
					if(!world.point_data.isPointComponentSlotActive(npc.slot_id)) continue;
					PointComponentSlot& slot = world.point_data.getPointComponentSlot(npc.slot_id);
					static_cast<HumanComponent*>(slot.getData())->desired_velocity = velocity;
				}
				profiler.beginTick();
				world.tick();
				profiler.endTick(tick + 1 == ticks);
			}

			PhaseResult result;
			for(const DiagnosticNpc& npc : npcs)
			{
				if(!world.point_data.isPointComponentSlotActive(npc.slot_id)) continue;
				const PointComponentSlot& slot = world.point_data.getPointComponentSlot(npc.slot_id);
				const auto& human = *static_cast<const HumanComponent*>(slot.getData());
				++result.active;
				if(human.grounded) ++result.grounded;
				if(glm::length(getNpcPosition(world, slot) - npc.start_position) > 0.001) ++result.moved;
			}
			formatLogInfo("NPC diagnostics result phase=% active=% grounded=% moved=%",
				name, result.active, result.grounded, result.moved);
			return result;
		}
	}

	/** @copydoc runNpcDiagnostics */
	int runNpcDiagnostics()
	{
		World scene;
		scene.camera.unbind();
		scene.factions.create(faction_definition_ids::celestial_body);
		scene.factions.create(faction_definition_ids::player);
		scene.factions.create(faction_definition_ids::pirates);

		const WorldBodyId body_id = scene.world_body_data.createWorldBody();
		PlanetData& planet = scene.planets.createPlanet(body_id, planet_settings_ids::earth);
		DiagnosticNpc npcs[npc_count]{};
		createNpcFixture(scene, body_id, planet, npcs);

		formatLogInfo("NPC diagnostics population=% PlayerFaction=% Pirates=%", npc_count, npc_count / 2, npc_count / 2);
		profiler.setReportIntervalMilliseconds(std::numeric_limits<uint64>::max());
		const PhaseResult warmup = measureNpcPhase(scene, npcs, "warmup", 60, vec3(0.0f));
		const PhaseResult idle = measureNpcPhase(scene, npcs, "idle", 120, vec3(0.0f));
		const PhaseResult walking = measureNpcPhase(scene, npcs, "walking", 120, vec3(1.5f, 0.0f, 0.0f));

		bool lost_support = false;
		double drop = 0.0;
		if(scene.point_data.isPointComponentSlotActive(npcs[0].slot_id))
		{
			const auto& before = scene.point_data.getPointComponentSlot(npcs[0].slot_id);
			const glm::dvec3 center = getNpcPosition(scene, before);
			planet.terrain_edits.append(
				PlanetTerrainEdit{center, 2.0, default_materials::void_material});
			++planet.terrain_revision;
			measureNpcPhase(scene, npcs, "excavation", 1, vec3(0.0f));

			if(scene.point_data.isPointComponentSlotActive(npcs[0].slot_id))
			{
				const auto& after = scene.point_data.getPointComponentSlot(npcs[0].slot_id);
				const auto& human = *static_cast<const HumanComponent*>(after.getData());
				drop = glm::dot(center - getNpcPosition(scene, after), glm::dvec3(human.up));
				lost_support = !human.grounded && drop > 0.0001;
			}
		}
		const bool passed = warmup.grounded == npc_count
			&& idle.grounded == npc_count && idle.moved == 0
			&& walking.grounded == npc_count && walking.moved == npc_count && lost_support;
		formatLogInfo("NPC diagnostics excavation lost_support=% drop_m=%", lost_support, drop);
		formatLogInfo("NPC diagnostics %", passed ? "PASS" : "FAIL");
		scene.destroyWorldBody(body_id);
		scene.point_data.release();
		scene.world_body_data.release();
		scene.factions.release();
		return passed ? EXIT_SUCCESS : EXIT_FAILURE;
	}
}
