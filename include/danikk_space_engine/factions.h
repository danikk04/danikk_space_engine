#pragma once

#include <danikk_space_engine/id.h>

namespace danikk_space_engine
{
	/** @brief Indices of built-in faction templates. */
	namespace faction_definition_ids
	{
		inline constexpr FactionDefinitionId celestial_body = 0;
		inline constexpr FactionDefinitionId player = 1;
		inline constexpr FactionDefinitionId pirates = 2;
	}

	/** @brief Faction indices initialized by the base scenario, in template order. */
	namespace base_faction_ids
	{
		inline constexpr FactionId celestial_body = 0;
		inline constexpr FactionId player = 1;
		inline constexpr FactionId pirates = 2;
	}

	/** @brief Sentinel for an entity with no faction assigned. */
	inline constexpr FactionId no_faction = static_cast<FactionId>(-1);
	/** @brief Immutable defaults used when creating a faction. */
	struct FactionDefinition
	{
		const char* default_name; ///< Initial faction name.
		vec3 default_color; ///< Initial faction color.
	};

	/** @brief Diplomatic state shared by both directions of a faction pair. */
	enum class FactionRelationType : uint8 { neutral, allied, hostile };

	/** @brief Diplomatic state and reputation for a faction pair. */
	struct FactionRelation
	{
		FactionRelationType type = FactionRelationType::neutral; ///< Current diplomatic state.
		float reputation = 0.0f; ///< Reputation; no automatic state transitions are applied.
	};

	/** @brief Faction state and relations indexed by the other faction's ID. */
	struct Faction
	{
		FactionDefinitionId definition_id = 0; ///< Index of the template used at creation.
		String name; ///< World-specific faction name.
		vec3 color = vec3(1.0f); ///< World-specific faction color.
	private:
		/** @brief Relations managed symmetrically by FactionSystem. */
		DynamicArray<FactionRelation> relations;
		friend class FactionSystem;
	};

	/** @brief Returns the number of built-in faction templates. */
	size_t getFactionDefinitionCount();
	/** @brief Returns a template by index; an invalid index terminates with a diagnostic. */
	const FactionDefinition& getFactionDefinition(FactionDefinitionId id);

	/** @brief Owns world-local factions, their relations, and faction-state lifecycle. */
	class FactionSystem
	{
		DynamicArray<Faction> factions;
	public:
		/** @brief Creates a faction from a template and returns its index. */
		FactionId create(FactionDefinitionId definition_id);
		/** @brief Returns the number of world factions. */
		size_t size() const;
		/** @brief Returns a faction by index; invalid IDs terminate with a diagnostic. */
		Faction& get(FactionId id);
		/** @brief Returns a read-only faction by index. */
		const Faction& get(FactionId id) const;
		/** @brief Returns a relation; a faction's relation to itself is allied. */
		FactionRelation getRelation(FactionId a, FactionId b) const;
		/** @brief Updates both directions of a relation; self-relations cannot be changed. */
		void setRelation(FactionId a, FactionId b, const FactionRelation& relation);
		/** @brief Removes all factions while retaining allocated storage. */
		void clear();
		/** @brief Removes all factions and releases their storage. */
		void release();
	};
}
