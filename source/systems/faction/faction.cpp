#include <danikk_space_engine/systems/faction/faction.h>
#include <danikk_framework/log.h>

namespace danikk_space_engine
{
	void FactionSystem::init()
	{
		danikk_framework::formatLogInfo("Init FactionSystem");
		clear();
	}
	void FactionSystem::shutdown()
	{
		danikk_framework::formatLogInfo("Shutdown FactionSystem");
		release();
	}
	namespace
	{
		const FactionDefinition definitions[] = {
			{"CelestialBody", vec3(0.6f, 0.6f, 0.6f)},
			{"PlayerFaction", vec3(0.3f, 0.6f, 1.0f)},
			{"Pirates", vec3(1.0f, 0.3f, 0.3f)}
		};
	}

	size_t getFactionDefinitionCount() { return sizeof(definitions) / sizeof(definitions[0]); }

	const FactionDefinition& getFactionDefinition(FactionDefinitionId id)
	{
		if(id >= getFactionDefinitionCount()) danikk_engine::formatFatalError("Invalid faction definition ID %", id);
		return definitions[id];
	}

	FactionId FactionSystem::create(FactionDefinitionId definition_id)
	{
		const FactionDefinition& definition = getFactionDefinition(definition_id);
		const size_t old_count = factions.size();
		const size_t new_count = old_count + 1;
		Faction faction;
		faction.definition_id = definition_id;
		faction.name = definition.default_name;
		faction.color = definition.default_color;
		faction.relations.resize(new_count);
		factions.push(static_cast<Faction&&>(faction));
		for(size_t index = 0; index < old_count; ++index)
			factions[index].relations.resize(new_count);
		return static_cast<FactionId>(old_count);
	}

	size_t FactionSystem::size() const { return factions.size(); }

	Faction& FactionSystem::get(FactionId id)
	{
		if(id >= factions.size()) danikk_engine::formatFatalError("Invalid faction ID %", id);
		return factions[id];
	}

	const Faction& FactionSystem::get(FactionId id) const
	{
		if(id >= factions.size()) danikk_engine::formatFatalError("Invalid faction ID %", id);
		return factions[id];
	}

	FactionRelation FactionSystem::getRelation(FactionId a, FactionId b) const
	{
		get(a);
		get(b);
		if(a == b) return {FactionRelationType::allied, 0.0f};
		return factions[a].relations[b];
	}

	void FactionSystem::setRelation(FactionId a, FactionId b, const FactionRelation& relation)
	{
		get(a);
		get(b);
		if(a == b) danikk_engine::formatFatalError("Cannot change a faction's relation to itself");
		factions[a].relations[b] = relation;
		factions[b].relations[a] = relation;
	}

	void FactionSystem::clear() { factions.clear(); }
	void FactionSystem::release()
	{
		factions = DynamicArray<Faction>();
	}
}
