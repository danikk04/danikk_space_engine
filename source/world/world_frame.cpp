#include <danikk_space_engine/profiler.h>
#include <danikk_space_engine/systems/component/component.h>
#include <danikk_space_engine/world.h>
#include <danikk_space_engine/effects/chunk_boundaries.h>
#include <danikk_space_engine/effects/unit_selection.h>
#include <danikk_space_engine/effects/unit_order_indicators.h>
#include <danikk_space_engine/effects/unit_activity_labels.h>
#include <danikk_space_engine/assets/collection.h>
#include <danikk_space_engine/config.h>
#include <danikk_engine/danikk_engine.h>

namespace danikk_space_engine
{
	/**
	 * @brief Accepts generated work, updates chunk/component render state, and draws the world frame.
	 *
		 * Completed chunks are accepted only when world, body, planet generation,
		 * and terrain revision still match. Dirty meshes are rebuilt on
	 * the main thread; rendering uses camera-relative transforms and optional
	 * debug effects are controlled by Settings.
	 */
	void World::frame()
	{
		/**
		 * @brief Measures the duration of the enclosing profiler scope.
		 */
		DSE_WATCH();
		setClearColor(camera.isActive()
			? planets.getAtmosphereBackgroundColor(camera.getWorldPosition()) : glm::vec3(0.0f));
		components.renderWorldBodyComponents();
		planets.update();
		chunk_mesh_generation.frame();
		static_asset_collection.base3d_shader.use();
		camera.frame();
		const vec3 camera_front = camera.getFront();
		const mat4 view = glm::lookAt(vec3(0.0f), camera_front, camera.getUp());
		setViewMatrix(view);
		chunks.renderWorld();
		// Draw dynamic chunk components after the world meshes so transparent overlays
		// do not allow later terrain draws to pass through the base mesh depth.
		components.renderChunkComponents();
		if(settings.show_chunk_bounds) drawChunkBoundaries(*this);
		drawUnitSelectionIndicators(*this);
		drawUnitOrderIndicators(*this);
		controller.renderPreview();
		drawUnitActivityLabels(*this);
	}
}
