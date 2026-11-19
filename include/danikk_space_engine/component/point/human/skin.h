#pragma once

#include <danikk_space_engine/id.h>
#include <danikk_engine/texture.h>

namespace danikk_space_engine
{
	namespace human_skin_ids
	{
		/** @brief ID of the empty fallback skin. */
		inline constexpr HumanSkinId none = 0;
		/** @brief ID of the bundled USMC MARPAT Woodland soldier skin. */
		inline constexpr HumanSkinId usmc_marpat_woodland = 1;
		/** @brief ID of the bundled Reimu Hakurei skin. */
		inline constexpr HumanSkinId reimu_hakurei = 2;
		/** @brief ID of the bundled Marisa Kirisame skin. */
		inline constexpr HumanSkinId marisa_kirisame = 3;
		/** @brief ID of the bundled Tsumi-bukuro skin. */
		inline constexpr HumanSkinId tsumi_bukuro = 4;
	}

	/** @brief Owns the name and texture asset for one Minecraft-style human skin. */
	struct HumanSkin
	{
		/** @brief Display/debug name of the skin. */
		const char* name = "";
		/** @brief Skin atlas texture; empty for the fallback skin. */
		danikk_engine::Texture texture;

		/** @brief Creates an empty fallback skin record. */
		HumanSkin() = default;
		/** @brief Loads one skin atlas with nearest-neighbor filtering. */
		HumanSkin(const char* name, const char* texture_name);
	};

	/** @brief Clears and loads the built-in human skin catalog, reserving ID zero for fallback. */
	void loadHumanSkins();
	/** @brief Returns the skin record at an index-based ID or fails for an invalid ID. */
	HumanSkin& getHumanSkin(HumanSkinId skin_id);
}
