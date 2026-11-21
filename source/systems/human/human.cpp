#include <danikk_space_engine/systems/human/system.h>
#include <danikk_space_engine/systems/telemetry/telemetry.h>
#include <danikk_space_engine/world.h>
#include <danikk_framework/log.h>

namespace danikk_space_engine
{
	void HumanSystem::init()
	{
		danikk_framework::formatLogInfo("Init HumanSystem");
		humans.clear();
		next_human_id = 0;
		next_generation_id = 1;
	}

	void HumanSystem::shutdown()
	{
		danikk_framework::formatLogInfo("Shutdown HumanSystem");
		humans.clear();
	}

	HumanData& HumanSystem::createHuman(WorldBodyId body_id,
		const glm::dvec3& body_local_position, const HumanState& initial_state)
	{
		if(!world.world_body_data.isWorldBodyActive(body_id))
			danikk_engine::formatFatalError("Cannot create a human on inactive WorldBody %", body_id);
		HumanData& human = humans.pushCtor();
		human = HumanData();
		human.id = next_human_id++;
		if(next_human_id == static_cast<HumanId>(-1))
			danikk_engine::formatFatalError("Human ID space is exhausted");
		human.generation_id = next_generation_id++;
		if(next_generation_id == 0) next_generation_id = 1;
		static_cast<HumanState&>(human) = initial_state;
		human.world_body_id = body_id;
		human.body_local_position = body_local_position;
		return human;
	}

	bool HumanSystem::destroyHuman(HumanId id)
	{
		for(size_t index = 0; index < humans.size(); ++index)
		{
			if(humans[index].id != id) continue;
			for(size_t shift = index + 1; shift < humans.size(); ++shift)
				humans[shift - 1] = static_cast<HumanData&&>(humans[shift]);
			humans.resize(humans.size() - 1);
			return true;
		}
		return false;
	}

	void HumanSystem::destroyWorldBodyHumans(WorldBodyId body_id)
	{
		for(size_t index = 0; index < humans.size();)
		{
			if(humans[index].world_body_id != body_id)
			{
				++index;
				continue;
			}
			for(size_t shift = index + 1; shift < humans.size(); ++shift)
				humans[shift - 1] = static_cast<HumanData&&>(humans[shift]);
			humans.resize(humans.size() - 1);
		}
	}

	bool HumanSystem::isHumanActive(HumanId id) const { return findHuman(id) != nullptr; }

	HumanData* HumanSystem::findHuman(HumanId id)
	{
		for(HumanData& human : humans) if(human.id == id) return &human;
		return nullptr;
	}

	const HumanData* HumanSystem::findHuman(HumanId id) const
	{
		for(const HumanData& human : humans) if(human.id == id) return &human;
		return nullptr;
	}

	HumanData* HumanSystem::findHumanAtPosition(WorldBodyId body_id,
		const glm::dvec3& body_local_position)
	{
		for(HumanData& human : humans)
			if(human.world_body_id == body_id && human.body_local_position == body_local_position) return &human;
		return nullptr;
	}

	glm::dvec3 HumanSystem::getWorldPosition(const HumanData& human) const
	{
		if(!world.world_body_data.isWorldBodyActive(human.world_body_id))
			danikk_engine::formatFatalError("Human % references inactive WorldBody %", human.id, human.world_body_id);
		const WorldBody& body = world.world_body_data.getWorldBody(human.world_body_id);
		return body.position + body.rotation * human.body_local_position;
	}

	void HumanSystem::tick()
	{
		DSE_WATCH();
		for(HumanData& human : humans) tickHumanData(human, world);
	}

	void HumanSystem::frame()
	{
		DSE_WATCH();
		for(const HumanData& human : humans) frameHumanData(human, world);
	}
}
