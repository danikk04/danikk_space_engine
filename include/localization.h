#pragma once

#include <danikk_framework/localization.h>

namespace game
{
	using danikk_framework::Localization;

    extern danikk_framework::Localization localization;

	void initLocaliztion();

#define localization game::localization
}
