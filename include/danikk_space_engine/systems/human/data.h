#pragma once

#include <danikk_space_engine/systems/human/human.h>
#include <danikk_space_engine/fwd.h>
#include <danikk_space_engine/component/point/inventory.h>
#include <danikk_space_engine/component/point/damageable.h>

namespace danikk_space_engine
{
	/** @brief Complete state for one person, owned and iterated by HumanSystem. */
	struct HumanData : HumanState
	{
		/** @brief Stable identity; records are not recycled while tactical memories may reference them. */
		HumanId id = static_cast<HumanId>(-1);
		/** @brief Incarnation token retained for stale tactical references. */
		uint64 generation_id = 1;
		/** @brief World body carrying this person. */
		WorldBodyId world_body_id = static_cast<WorldBodyId>(-1);
		/** @brief Position relative to the owning body's origin. */
		glm::dvec3 body_local_position = glm::dvec3(0.0);
		/** @brief Inventory retained with the person, including when they die. */
		InventoryComponent inventory;
		/** @brief Health retained with the person. */
		DamageableComponent damageable;
	};

	void tickHumanData(HumanData& human, World& world);
	void frameHumanData(const HumanData& human, World& world);
}
