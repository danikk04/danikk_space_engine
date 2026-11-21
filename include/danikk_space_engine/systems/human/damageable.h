#pragma once

#include <danikk_space_engine/id.h>

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

	/** @brief Current health for one entity. */
	struct DamageableComponent
	{
		DamageableDefinitionId definition_id = damageable_definition_ids::human; ///< Shared settings index.
	private:
		float health = 100.0f; ///< Remaining health; changed through setHealth().
	public:
		/** @brief Returns the remaining health. */
		float getHealth() const { return health; }
		/** @brief Sets remaining health, clamped to zero. */
		void setHealth(float value);
	};

	/** @brief Returns the immutable settings at a valid definition index. */
	const DamageableDefinition& getDamageableDefinition(DamageableDefinitionId id);
	/** @brief Applies ballistic resistance and clamps remaining health to zero. */
	void applyBallisticDamage(DamageableComponent& damageable, float damage);
}
