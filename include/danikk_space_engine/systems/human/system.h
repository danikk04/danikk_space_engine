#pragma once

#include <danikk_framework/dynamic_array.h>
#include <danikk_space_engine/systems/human/data.h>

namespace danikk_space_engine
{
	/** @brief Owns human entities, their body-local positions, health, inventory, and lifecycle. */
	class HumanSystem
	{
		World& world;
		DynamicArray<HumanData> humans;
		HumanId next_human_id = 0;
		uint64 next_generation_id = 1;

	public:
		explicit HumanSystem(World& world) : world(world) {}
		void init();
		void shutdown();
		HumanData& createHuman(WorldBodyId body_id, const glm::dvec3& body_local_position,
			const HumanState& initial_state = HumanState());
		/** @brief Creates a soldier at a feet position, equips its default weapon, and registers its AI. */
		HumanId createNpc(WorldBodyId body_id, const glm::dvec3& feet_position,
			const vec3& up, const vec3& forward, FactionId faction_id);
		bool destroyHuman(HumanId id);
		void destroyWorldBodyHumans(WorldBodyId body_id);
		bool isHumanActive(HumanId id) const;
		HumanData* findHuman(HumanId id);
		const HumanData* findHuman(HumanId id) const;
		const DynamicArray<HumanData>& getHumans() const { return humans; }
		void tick();
		void frame();
		glm::dvec3 getWorldPosition(const HumanData& human) const;
	};
}
