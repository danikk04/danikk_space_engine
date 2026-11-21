#pragma once

#include <danikk_space_engine/default.h>
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
		weapon_ready, ///< Right arm points forward and left arm is rotated inward to support a weapon.
		corpse_spread ///< Arms are rotated outward after death.
	};

	/** @brief Selects the human's overall body pose, including the settled pose of a corpse. */
	enum class HumanBodyPose : uint8
	{
		standing, ///< Upright pose used while alive.
		lying_on_back, ///< Horizontal pose with the face turned upward.
		lying_on_stomach ///< Horizontal pose with the face turned downward.
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
		/** @brief Alternate three-pixel arm mesh; populated only for arm parts. */
		Mesh slim_mesh;
		/** @brief Alternate three-pixel arm overlay mesh; populated only for arm parts. */
		Mesh slim_overlay_mesh;
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
	 * `forward`, `up`, and the one-tick `desired_velocity` are in the owning WorldBody's local axes.
	 * UnitGroup or player control writes the velocity command; HumanSystem consumes and clears it, updates
	 * gravity/support state, and advances the visual walking pose from actual tangential travel.
	 */
	struct HumanState
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
			/** @brief Instance token of the planet component that supplied the field. */
			uint64 planet_generation_id = 0;
			/** @brief Terrain edit revision used by the support probe. */
			uint64 terrain_revision = 0;
		};


		/** @brief Faction of this person, independent of the body they stand on. */
		FactionId faction_id = static_cast<FactionId>(-1);
		/** @brief Skin/model choice; the attached point remains the vertical center of the body. */
		HumanAppearance appearance;
		/** @brief Current arm animation mode, also used to attach equipped weapons. */
		HumanArmPose arm_pose = HumanArmPose::relaxed;
		/** @brief Per-arm corpse spread angles in radians, sampled once when the human dies. */
		vec2 corpse_arm_spread_angles = vec2(0.0f);
		/** @brief Overall body pose; death selects and retains one of the two lying poses. */
		HumanBodyPose body_pose = HumanBodyPose::standing;
		/** @brief Local direction opposite gravity; defines feet and the walking plane. */
		vec3 up = vec3(0.0f, 0.0f, 1.0f);
		/** @brief Last confirmed ground normal in owning-body coordinates, used to orient corpses to slopes. */
		vec3 ground_normal = vec3(0.0f, 0.0f, 1.0f);
		/**
		 * @brief One-tick movement command in owning-body coordinates, in metres per second.
		 * The human tick consumes and resets this command after applying movement.
		 */
		vec3 desired_velocity = vec3(0.0f);
	private:
		/** @brief Current body orientation in body-local coordinates. */
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
		/** @brief Requests a body-local look direction and optional target distance for the next update. */
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
		/** @brief Temporary marker used by PhysicsSystem to restore ground clearance after human collisions. */
		bool displaced_by_human_collision = false;
	};

	/**
	 * @brief Initializes shared human part dimensions, pivots, colors, and base/overlay meshes.
	 * @param definition Definition object to populate for later shared rendering.
	 */
	void loadHumanDefinition(HumanDefinition& definition);

	/**
	 * @brief Dispatches the selected renderer and draws the shared body parts with the chosen skin.
	 * @param human Character pose and appearance selection.
	 * @param world_matrix Transform from character-local coordinates to render coordinates.
	 * @param color_tint Multiplied with skin/part colors; alpha controls preview translucency.
	 */
	void drawHumanAppearance(const HumanState& human, const mat4& world_matrix,
		const vec4& color_tint = vec4(1.0f));
}
