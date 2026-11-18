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
	 * index in the loaded human-skin catalog. The model path is reserved; rendering currently
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
		/** @brief Records the inputs for which the planet field last confirmed ground support. */
		struct GroundSupportCache
		{
			/** @brief Supported capsule center relative to the planet body. */
			glm::dvec3 center = glm::dvec3(0.0);
			/** @brief Up axis used by the support probe, in body coordinates. */
			vec3 up = vec3(0.0f);
			/** @brief Height used by the support probe. */
			float height = 0.0f;
			/** @brief Chunk owning the supported point, or the unattached sentinel for an empty cache. */
			ChunkId chunk_id = PointComponentSlot::no_chunk;
			/** @brief Instance token of the planet component that supplied the field. */
			uint64 planet_generation_id = 0;
			/** @brief Terrain edit revision used by the support probe. */
			uint64 terrain_revision = 0;
		};

		/** @brief Tracks capsule changes and pending checks for human-to-human collisions. */
		struct CollisionActivity
		{
			/** @brief Capsule center at the start of the previous collision tick, in world coordinates. */
			glm::dvec3 center = glm::dvec3(0.0);
			/** @brief World-space up axis sampled with the center. */
			glm::dvec3 up = glm::dvec3(0.0);
			/** @brief Owning chunk at the previous sample, or the unattached sentinel before the first check. */
			ChunkId chunk_id = PointComponentSlot::no_chunk;
			/** @brief Capsule height at the previous sample. */
			float height = 0.0f;
			/** @brief True when this capsule needs contact checks during the current collision tick. */
			bool active = true;
			/** @brief Keeps contacts eligible next tick after a correction or an unresolved overlap. */
			bool recheck_next_tick = false;
		};

		/** @brief Faction of this person, independent of the body they stand on. */
		FactionId faction_id = static_cast<FactionId>(-1);
		/** @brief Skin/model choice; the attached point remains the vertical center of the body. */
		HumanAppearance appearance;
		/** @brief Current arm animation mode, also used to attach equipped weapons. */
		HumanArmPose arm_pose = HumanArmPose::relaxed;
		/** @brief Local direction opposite gravity; defines feet and the walking plane. */
		vec3 up = vec3(0.0f, 0.0f, 1.0f);
		/**
		 * @brief One-tick movement command in owning-chunk coordinates, in metres per second.
		 * The human tick consumes and resets this command after applying movement.
		 */
		vec3 desired_velocity = vec3(0.0f);
	private:
		/** @brief Current body orientation in chunk-local coordinates. */
		vec3 forward = vec3(0.0f, 1.0f, 0.0f);
		/** @brief Vertical head look angle, independent from weapon elevation. */
		float head_pitch = 0.0f;
		/** @brief Vertical weapon aim relative to the body's forward direction, in radians. */
		float weapon_aim_pitch = 0.0f;
		vec3 target_look_direction = vec3(0.0f);
		/** @brief Distance from the head look origin to its target, when known. */
		float target_look_distance = 0.0f;
		/** @brief First death timestamp in engine milliseconds; zero means no death recorded. */
		uint32 death_timestamp_ms = 0;
		/** @brief Default height in metres before modifiers. */
		static constexpr float base_height = 1.8f;
		/** @brief Default walking speed in metres per second before modifiers. */
		static constexpr float base_movement_speed = 1.5f;
		/** @brief Default firing deviation contributed by the person, in degrees before skills. */
		static constexpr float base_spread_degrees = 1.0f;
	public:
		/** @brief Returns the current body-facing direction. */
		vec3 getForward() const { return forward; }
		/** @brief Returns the head's independent vertical look angle. */
		float getHeadPitch() const { return head_pitch; }
		/** @brief Returns the weapon's vertical aim relative to the body. */
		float getWeaponAimPitch() const { return weapon_aim_pitch; }
		/** @brief Sets the initial body-facing direction before the human is spawned. */
		void setInitialForward(const vec3& direction);
		/** @brief Requests a chunk-local look direction and optional target distance for the next update. */
		void lookAtDirection(const vec3& direction, float target_distance = 0.0f);
		/** @brief Returns the direction in which the currently held weapon points. */
		vec3 getWeaponDirection() const;
		/** @brief Advances body and head orientation toward the latest request or movement direction. */
		void updateOrientation(const vec3& displacement, float dt);
		/** @brief Returns character height; modifiers can be applied here in the future. */
		float getHeight() const { return base_height; }
		/** @brief Returns walking speed; modifiers can be applied here in the future. */
		float getMovementSpeed() const { return base_movement_speed; }
		/** @brief Returns firing spread before skill modifiers are applied. */
		float getSpreadDegrees() const { return base_spread_degrees; }
		/** @brief Records the first death timestamp without replacing it on later ticks. */
		void startDeathTimestamp(uint32 timestamp_ms);
		/** @brief Clears the stored death time if the human becomes alive again. */
		void clearDeathTimestamp();
		/** @brief Reports whether this human has a recorded nonzero death timestamp. */
		bool hasDeathTimestamp() const { return death_timestamp_ms != 0; }
		/** @brief Returns elapsed milliseconds since death using wrap-safe uint32 subtraction. */
		uint32 getDeathElapsedMilliseconds() const;
		/** @brief Accumulated walking animation phase, advanced by actual ground travel. */
		float walk_phase = 0.0f;
		/** @brief Current leg-swing magnitude derived from movement speed. */
		float walk_amplitude = 0.0f;
		/** @brief Velocity along `up`; negative values indicate falling. */
		float vertical_speed = 0.0f;
		/** @brief True when the current tick confirmed or reused a supporting surface. */
		bool grounded = false;
		/** @brief Enables support probing, slope-constrained walking, and falling integration. */
		bool follow_ground = true;
		/** @brief Reusable support query result for the planet's scalar field. */
		GroundSupportCache ground_support;
		/** @brief Collision activity owned by the human contact solver. */
		CollisionActivity collision_activity;
		/** @brief Temporary marker used by WorldPhysics to restore ground clearance after human collisions. */
		bool displaced_by_human_collision = false;
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
