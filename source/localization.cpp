#include <danikk_engine/localization.h>

#define _ set_localization

using namespace danikk_framework;

namespace danikk_space_engine
{
	danikk_framework::Localization localization;

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
