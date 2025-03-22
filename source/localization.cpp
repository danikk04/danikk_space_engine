#include <danikk_engine/localization.h>

#define _ set_localization

using namespace danikk_framework;

namespace danikk_engine
{

	danikk_framework::Localization localization;

	void initLocaliztion()
	{
		_("air",								"Air",						"Воздух");
		_("loght_armor",						"Light armor",				"Лёгкая броня");
		_("wooden_container",					"Wooden container",			"Деревянный ящик");
	}
}
