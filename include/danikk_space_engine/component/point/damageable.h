#pragma once

#include <danikk_space_engine/component/point/point.h>

namespace danikk_space_engine
{
	/** @brief Indices into the shared damageable-definition table. */
	namespace damageable_definition_ids
	{
		inline constexpr DamageableDefinitionId human = 0;
		inline constexpr DamageableDefinitionId build_part = 1;
	}

	/** @brief Base health and fractional damage resistances for a kind of object. */
	struct DamageableDefinition
	{
		float max_health; ///< Maximum health for a human or a one-cubic-metre part.
		float ballistic_resistance; ///< Fraction of ballistic damage absorbed, from zero to one.
		float explosive_resistance; ///< Fraction of explosive damage absorbed.
		float thermal_resistance; ///< Fraction of thermal damage absorbed.
	};

	/** @brief Current health for one point-attached object. */
	struct DamageableComponent
	{
		DamageableDefinitionId definition_id = damageable_definition_ids::human; ///< Shared settings index.
	private:
		float health = 100.0f; ///< Remaining health; changed through setHealth().
	public:
		/** @brief Returns the remaining health. */
		float getHealth() const { return health; }
		/** @brief Sets remaining health, clamped to zero. */
		void setHealth(float value, PointId point_id);
	};

	/** @brief Returns the immutable settings at a valid definition index. */
	const DamageableDefinition& getDamageableDefinition(DamageableDefinitionId id);
	/** @brief Returns the damageable component on a point in the specified chunk, or null. */
	DamageableComponent* findDamageable(World& world, ChunkId chunk_id, PointId point_id);
	/** @brief Applies ballistic resistance and clamps remaining health to zero. */
	void applyBallisticDamage(DamageableComponent& damageable, float damage, PointId point_id);
	/** @brief Constructs a damageable payload using optional initial state. */
	void constructDamageableComponent(void* destination, const void* init_data);
	/** @brief Moves a damageable payload to replacement storage. */
	void moveDamageableComponent(void* destination, void* source);
	/** @brief Destroys a damageable payload. */
	void destroyDamageableComponent(void* data);
}
