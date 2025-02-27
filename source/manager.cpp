#include <manager.h>

namespace sotiria_project
{
	Manager game_manager;

	void Manager::tick()
	{
		for(Object& o : map_root.childs)
		{
			o.tick(map_root);
		}
	}

	void Manager::frame()
	{
		for(Object& o : map_root.childs)
		{
			o.frame(map_root);
		}
	}

	void Manager::init()
	{

	}
}
