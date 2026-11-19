#include <danikk_framework/glm.h>

#include <danikk_space_engine/profiler.h>
#include <danikk_space_engine/systems/component/component.h>
#include <danikk_space_engine/world.h>
#include <danikk_space_engine/systems/unit/unit.h>
#include <danikk_space_engine/systems/controller/controller.h>

namespace danikk_space_engine
{
	/**
	 * @brief Ticks body and chunk components, drains deferred point deletion, then advances physics.
	 * Body, chunk, and per-body component IDs are snapshotted before callbacks can mutate ownership lists.
	 * Deferred cleanup completes before UnitSystem updates faction intelligence and orders. PhysicsSystem
	 * then advances motion and resolves contacts.
	 */
	void World::tick()
	{
		/**
		 * @brief Measures the duration of the enclosing profiler scope.
		 */
		DSE_WATCH();
		components.tickWorldBodyComponents();
		camera.tick();
		controller.tick();
		components.flushDeferredPointDestruction();
		unit_system.tick(glm::max(getTargetFrameDelay(), 0.0f));
		physics.tick();
	}
}
