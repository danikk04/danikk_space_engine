#pragma once

#include <danikk_framework/localization.h>

namespace danikk_space_engine
{
	using danikk_framework::Localization;

    extern danikk_framework::Localization localization;

	void initLocaliztion();

#define localization danikk_space_engine::localization
}
