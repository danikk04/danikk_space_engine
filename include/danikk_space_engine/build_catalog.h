#pragma once

#include <danikk_space_engine/id.h>
#include <danikk_space_engine/tool.h>

namespace danikk_space_engine
{
	/** @brief Stable indexes for entries in the buildable-detail catalog. */
	namespace build_definition_ids
	{
		inline constexpr BuildDefinitionId none = 0; ///< Empty hotbar slot.
		inline constexpr BuildDefinitionId cube = 1; ///< Unit cube BuildPart.
		inline constexpr BuildDefinitionId slope = 2; ///< Slope BuildPart.
		inline constexpr BuildDefinitionId machinegun = 3; ///< MG-42 placement.
		inline constexpr BuildDefinitionId count = 4; ///< Number of catalog entries, including none.
	}

	/** @brief Top-level categories displayed by the build catalog. */
	enum class BuildCategoryId : uint8
	{
		structural = 0, ///< Cube and slope construction pieces.
		weapons, ///< Placeable weapons.
		count ///< Number of categories.
	};

	/** @brief Immutable UI and placement data for one buildable detail. */
	struct BuildDefinition
	{
		/** @brief Stable catalog ID stored in a hotbar slot. */
		BuildDefinitionId id = build_definition_ids::none;
		/** @brief Category used to filter this entry in the selection panel. */
		BuildCategoryId category = BuildCategoryId::structural;
		/** @brief Full label shown in the build catalog. */
		const char* name = "None";
		/** @brief Compact label shown inside an assigned hotbar slot. */
		const char* short_name = "";
		/** @brief Tool category activated by this detail. */
		ToolId tool = ToolId::none;
		/** @brief Tool action used when this detail is placed. */
		ToolActionId action = ToolActionId::none;
	};

	/**
	 * @brief Returns immutable data for one buildable detail.
	 * @param id Stable ID from `build_definition_ids`.
	 * @return Catalog definition; an invalid ID triggers a fatal diagnostic.
	 */
	const BuildDefinition& getBuildDefinition(BuildDefinitionId id);

	/**
	 * @brief Counts definitions visible in one build-catalog category.
	 * @param category Category to enumerate.
	 * @return Number of detail entries in that category.
	 */
	uint32 getBuildDefinitionCount(BuildCategoryId category);

	/**
	 * @brief Returns one definition ID from a category's display order.
	 * @param category Category to enumerate.
	 * @param index Zero-based position within that category.
	 * @return Definition ID; an invalid index triggers a fatal diagnostic.
	 */
	BuildDefinitionId getBuildDefinitionId(BuildCategoryId category, uint32 index);

	/**
	 * @brief Returns the localized-ready display label for one category.
	 * @param category Category to name.
	 * @return Static category label or "Unknown" for an invalid category.
	 */
	const char* getBuildCategoryName(BuildCategoryId category);
}
