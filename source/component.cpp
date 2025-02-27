#include <default.h>
#include <component/component.h>

namespace sotiria_project
{
	ComponentClassStorage::ComponentClassStorage()
	{
		pushCtor();//0 индекс - пропуск
	}

	ComponentClassStorage component_class_storage;
}
