#pragma once

#include <object/world.h>

namespace danikk_space_engine
{
	class StructureObject : public WorldObject
	{
		DynamicArray<StructureObject*> links;
	public:
		void tick()
		{

		}
	};
}
