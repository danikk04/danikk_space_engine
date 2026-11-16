#include <danikk_engine/localization.h>

#define _ set_localization

using namespace danikk_framework;

namespace danikk_space_engine
{
	danikk_framework::Localization localization;

	/**
	 * @brief Registers the current built-in block names in English and Russian.
	 *
	 * This module adds entries to the shared framework catalog; it does not load
	 * a language file or select the active locale. Keys are grouped under `block/`
	 * and passed to the framework localization-registration macro. The public
	 * function name keeps the existing misspelling for compatibility with its call sites.
	 */
	void initLocaliztion()
	{
		_("block/air",								"Air",						"Воздух");
		_("block/vacuum",							"Vacuum",					"Вакуум");
		_("block/loght_armor",						"Light armor",				"Лёгкая броня");
		_("block/wooden_container",					"Wooden container",			"Деревянный ящик");
		_("block/solid",							"Solid material",			"Твёрдый материал");
		_("block/fluid",							"Fluid",					"Жидкость");
	}
}
