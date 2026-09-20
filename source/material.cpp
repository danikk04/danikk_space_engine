#include <material.h>

#include <danikk_framework/cstring_functions.h>

namespace danikk_space_engine
{
	DynamicArray<Material> material_list;

	uint32 getMaterialID(const char* name)
	{
		for(index_t i = 0; i < material_list.size(); i ++)
		{
			if(strequal(material_list[i].name, name))
			{
				return i;
			}
		}
		return 0;
	}

	void initMaterials()
	{
		material_list.pushCtor("VOID");
		material_list.pushCtor("sandstone");
		material_list.pushCtor("granite");
	}
}
