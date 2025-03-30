#include <object/block_map.h>
#include <danikk_engine/dynamic_mesh.h>

namespace game
{
	BlockMapRegion& RegionContainer::operator[](const uvec3& key)
	{
		for(data_t& element : data)
		{
			if(element.key == key)
			{
				return element.region;
			}
		}
		return data.pushCtor().region;
	}

	void BlockMapChunk::regenerateMesh()
	{
		if(!mesh.isNull())
		{
			mesh.clear();
		}

		DynamicMesh dynamic_mesh;

	}

	void BlockMapChunk::tick()
	{

	}

	void BlockMapChunk::frame()
	{

	}

	void BlockMapRegion::tick()
	{

	}

	void BlockMapRegion::frame()
	{

	}

	void RegionContainer::tick()
	{

	}

	void RegionContainer::frame()
	{

	}
}
