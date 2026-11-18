#pragma once

#include <danikk_space_engine/component/point/point.h>
#include <danikk_space_engine/geometry/build_part.h>

namespace danikk_space_engine
{
	/** @brief Invalid parent marker for a root BuildPartComponent. */
	constexpr PointComponentSlotId no_build_part_parent = static_cast<PointComponentSlotId>(-1);

	/**
	 * @brief Structural building part anchored to a point and attached to another part or the body root.
	 */
	struct BuildPartComponent
	{
		/** @brief Shape definition used for both rendering and collision. */
		BuildPartShape shape = BuildPartShape::cube;
		/** @brief Parent BuildPart slot, or no_build_part_parent for the body's root part. */
		PointComponentSlotId parent_slot_id = no_build_part_parent;
		/** @brief Rotation of the shape relative to its anchor point and body. */
		mat4 rotation = mat4(1.0f);
		/** @brief Per-axis dimensions in metres, scaling the unit local shape. */
		vec3 size = vec3(1.0f);
		/** @brief Construction completion in the inclusive range [0, 1]. */
		float construction_progress = 1.0f;
	};

	/** @brief Placement-constructs a BuildPart payload from optional initialization data. */
	void constructBuildPartComponent(void* destination, const void* init_data);
	/** @brief Relocates a BuildPart payload into replacement component-slot storage. */
	void moveBuildPartComponent(void* destination, void* source);
	/** @brief Ends the lifetime of a BuildPart payload. */
	void destroyBuildPartComponent(void* data);
	/** @brief Draws the BuildPart shape at its anchor point. */
	void frameBuildPartComponent(PointComponentSlot& slot, World& world);
	/** @brief Releases cached BuildPart meshes before graphics shutdown. */
	void clearBuildPartMeshCache();
}
