#include <danikk_space_engine/component/point/human/skin.h>

#include <danikk_space_engine/default.h>

namespace danikk_space_engine
{
	namespace
	{
		DynamicArray<HumanSkin> human_skins;
	}

	HumanSkin::HumanSkin(const char* name, const char* texture_name)
		: name(name), texture(texture_name, texture_filters::nearest)
	{}

	void loadHumanSkins()
	{
		human_skins.clear();
		human_skins.pushCtor();
		human_skins.pushCtor("USMC MARPAT Woodland",
			"minecraft_skins/medium-loadout-marpat-woodland-14906056-e1810");
		human_skins.pushCtor("Reimu Hakurei", "minecraft_skins/touhou_reimu");
		human_skins.pushCtor("Marisa Kirisame", "minecraft_skins/touhou_marisa");
		human_skins.pushCtor("Tsumi-bukuro", "minecraft_skins/touhou_tsumi_bukuro");
	}

	HumanSkin& getHumanSkin(HumanSkinId skin_id)
	{
		if(skin_id >= human_skins.size()) formatFatalError("Invalid Minecraft human skin ID %", skin_id);
		return human_skins[skin_id];
	}
}
