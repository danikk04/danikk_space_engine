#pragma once

#include <object/structure.h>

namespace danikk_space_engine
{
	class StructureGroupObject : public WorldObject
	{
		DynamicArray<StructureObject> links;
	public:
		void tick()
		{

		}
	};
}
