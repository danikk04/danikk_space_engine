#pragma once

#include <danikk_framework/localization.h>

namespace danikk_space_engine
{
	/** @brief Framework localization catalog populated with built-in DSE strings. */
	using danikk_framework::Localization;

	/** @brief Process-wide localization catalog used by the application. */
	extern danikk_framework::Localization localization;

	/**
	 * @brief Registers the built-in block/material names with the framework localization catalog.
	 *
	 * The function name retains its historical `Localiztion` misspelling. This
	 * registers strings only; it does not load a language file or select the active
	 * locale. The exported `localization` object is owned by this module.
	 */
	void initLocaliztion();

/** @brief Legacy shorthand that expands to the process-wide DSE localization object. */
#define localization danikk_space_engine::localization
}
