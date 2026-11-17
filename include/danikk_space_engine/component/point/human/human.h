#pragma once

#include <danikk_space_engine/component/point/point.h>
#include <danikk_space_engine/id.h>
#include <danikk_engine/mesh.h>

namespace danikk_space_engine
{
	/**
	 * @brief Selects the geometry and texture path used to render a human.
	 *
	 * The Minecraft path uses the shared six-part cuboid rig and interprets `skin_id` as an
	 * index in the loaded skin texture array. The model path is reserved; rendering currently
	 * warns once and falls back to the default Minecraft-style appearance.
	 */
	enum class HumanRenderType : uint8
	{
		minecraft_style, ///< Render six block-shaped parts with a Minecraft skin atlas.
		model_3d ///< Reserved path for a full model; currently falls back to the default human.
	};

	/**
	 * @brief Selects a human rendering style and the skin or model used by that style.
	 */
	struct HumanAppearance
	{
		/** @brief Rendering path selected for this character. */
		HumanRenderType render_type = HumanRenderType::minecraft_style;
		/** @brief Skin or model index interpreted according to `render_type`. */
		HumanSkinId skin_id = 0;
	};

	/** @brief Stable indices for the six independently transformed human body parts. */
	enum class HumanPart : uint8
	{
		head, ///< Head and face region of the skin atlas.
		torso, ///< Main body section.
		right_arm, ///< Character's right arm.
		left_arm, ///< Character's left arm.
		right_leg, ///< Character's right leg.
		left_leg, ///< Character's left leg.
		count ///< Number of concrete parts and array length for HumanDefinition::parts.
	};

	/** @brief Selects whether the arms swing naturally or form a weapon-holding pose. */
	enum class HumanArmPose : uint8
	{
		relaxed, ///< Arms swing with the walking cycle.
		weapon_ready ///< Right arm points forward and left arm is rotated inward to support a weapon.
	};

	/**
	 * @brief Describes one human body part mesh and its local transform.
	 */
	struct HumanPartDefinition
	{
		/** @brief Shared base mesh for this body part. */
		Mesh mesh;
		/** @brief Shared outer-layer mesh used for Minecraft skin overlays. */
		Mesh overlay_mesh;
		/** @brief Part dimensions as proportions of the character's total height. */
		vec3 size = vec3(1.0f);
		/** @brief Local joint location about which animation rotates this part. */
		vec3 pivot = vec3(0.0f);
		/** @brief Offset from the joint to the part mesh center, in height-relative units. */
		vec3 center_offset = vec3(0.0f);
		/** @brief Tint used when the selected skin has no texture. */
		vec3 color = vec3(1.0f);
	};

	/**
	 * @brief Shared render geometry and proportions used by all human instances.
	 *
	 * The six entries are indexed by HumanPart. Meshes live here once and are reused by every
	 * character; dimensions, pivots, and center offsets are expressed relative to body height.
	 */
	struct HumanDefinition
	{
		/** @brief Definitions indexed by HumanPart; meshes are shared and not copied per character. */
		HumanPartDefinition parts[static_cast<size_t>(HumanPart::count)];
	};

	/**
	 * @brief Stores a human character’s state, orientation, pose, and appearance.
	 *
	 * The owning Point is at the vertical center of the character. `forward`, `up`, and the
	 * one-tick `desired_velocity` are in the owning Chunk's local axes. NPC or player control
	 * writes the velocity command; this component consumes and clears it during its tick, updates
	 * gravity/support state, and advances the visual walking pose from actual tangential travel.
	 */
	struct HumanComponent
	{
		/** @brief Faction of this person, independent of the body they stand on. */
		FactionId faction_id = static_cast<FactionId>(-1);
		/** @brief Skin/model choice; the attached point remains the vertical center of the body. */
		HumanAppearance appearance;
		/** @brief Current arm animation mode, also used to attach equipped weapons. */
		HumanArmPose arm_pose = HumanArmPose::relaxed;
		/** @brief Character forward direction in owning-chunk coordinates. */
		vec3 forward = vec3(0.0f, 1.0f, 0.0f);
		/** @brief Local direction opposite gravity; defines feet and the walking plane. */
		vec3 up = vec3(0.0f, 0.0f, 1.0f);
		/**
		 * @brief One-tick movement command in owning-chunk coordinates, in metres per second.
		 * The human tick consumes and resets this command after applying movement.
		 */
		vec3 desired_velocity = vec3(0.0f);
		/** @brief Head yaw and pitch relative to the torso, in radians. */
		vec2 head_rotation = vec2(0.0f);
		/** @brief Full character height in world units; the point origin is half this height above the feet. */
		float height = 1.8f;
		/** @brief Maximum tangential walking speed in world units per second. */
		float movement_speed = 1.5f;
		/** @brief Accumulated walking animation phase, advanced by actual ground travel. */
		float walk_phase = 0.0f;
		/** @brief Current leg-swing magnitude derived from movement speed. */
		float walk_amplitude = 0.0f;
		/** @brief Velocity along `up`; negative values indicate falling. */
		float vertical_speed = 0.0f;
		/** @brief True when the current tick found a supporting surface beneath the character. */
		bool grounded = false;
		/** @brief Enables support probing, slope-constrained walking, and falling integration. */
		bool follow_ground = true;
	};

	/**
	 * @brief Initializes shared human part dimensions, pivots, colors, and base/overlay meshes.
	 * @param definition Definition object to populate for later shared rendering.
	 */
	void loadHumanDefinition(HumanDefinition& definition);

	/**
	 * @brief Creates a human at a world-body-relative position and equips it with the soldier loadout.
	 *
	 * Creates a point in an existing chunk and attaches the human and inventory components,
	 * selects the loaded USMC MARPAT Woodland skin, and equips the initial MG-42 stack.
	 * @param world World whose resource pools receive the new point, chunk, and component slots.
	 * @param chunk_id Existing chunk that owns the new point and components.
	 * @param position Feet position in world-body-local coordinates.
	 * @param up Initial local up direction.
	 * @param forward Initial local facing direction.
	 * @return ID of the new human component slot.
	 */
	PointComponentSlotId createHumanSoldier(World& world, ChunkId chunk_id,
		const glm::dvec3& position, const vec3& up, const vec3& forward);

	/**
	 * @brief Placement-constructs a HumanComponent from optional initial state.
	 * @param destination Component payload storage supplied by the slot pool.
	 * @param init_data Optional HumanComponent to copy; null uses default state.
	 */
	void constructHumanComponent(void* destination, const void* init_data);

	/**
	 * @brief Move-constructs a HumanComponent during point-slot relocation.
	 * @param destination Uninitialized replacement payload storage.
	 * @param source Existing payload, destroyed after the move.
	 */
	void moveHumanComponent(void* destination, void* source);

	/**
	 * @brief Runs the HumanComponent destructor without freeing slot-owned storage.
	 * @param data Address of the live payload.
	 */
	void destroyHumanComponent(void* data);

	/**
	 * @brief Consumes the movement command, applies ground/gravity motion, and updates facing/animation.
	 * @param slot Human component slot, including its point and owning chunk IDs.
	 * @param world World used for gravity, terrain support tests, and coordinate conversion.
	 */
	void tickHumanComponent(PointComponentSlot& slot, World& world);

	/**
	 * @brief Draws the human appearance and any equipped item using the current pose.
	 * @param slot Human component slot used to locate the point, chunk, and inventory.
	 * @param world World used to build camera-relative transforms.
	 */
	void frameHumanComponent(PointComponentSlot& slot, World& world);

	/**
	 * @brief Dispatches the selected renderer and draws the shared body parts with the chosen skin.
	 * @param human Character pose and appearance selection.
	 * @param world_matrix Transform from character-local coordinates to render coordinates.
	 */
	void drawHumanAppearance(const HumanComponent& human, const mat4& world_matrix);
}
