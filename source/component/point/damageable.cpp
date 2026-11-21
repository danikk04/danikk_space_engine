#include <danikk_space_engine/component/point/damageable.h>
#include <danikk_space_engine/world.h>

namespace danikk_space_engine
{
	namespace
	{
		const DamageableDefinition definitions[] = {
			{100.0f, 0.0f, 0.0f, 0.0f},
			{100.0f, 0.0f, 0.0f, 0.0f}
		};
	}

	const DamageableDefinition& getDamageableDefinition(DamageableDefinitionId id)
	{
		if(id >= sizeof(definitions) / sizeof(definitions[0]))
			formatFatalError("Invalid damageable definition ID %", id);
		return definitions[id];
	}

	void DamageableComponent::setHealth(float value)
	{
		const float next = glm::max(0.0f, value);
		if(next == health) return;
		health = next;
	}

	void applyBallisticDamage(DamageableComponent& damageable, float damage)
	{
		const float resistance = glm::clamp(
			getDamageableDefinition(damageable.definition_id).ballistic_resistance, 0.0f, 1.0f);
		damageable.setHealth(damageable.getHealth() - glm::max(0.0f, damage) * (1.0f - resistance));
	}
}
