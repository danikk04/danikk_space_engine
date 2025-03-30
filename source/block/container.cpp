#include <default.h>
#include <danikk_framework/string.h>
#include <danikk_engine/sprite.h>
#include <localization.h>

#include <block/block.h>
#include <block/container.h>

namespace game
{
	namespace block
	{
		size_t WoodenContainer::id;

		size_t AbstractContainer::variablesSize()
		{
			return 0 + Block::variablesSize();
		}

		size_t WoodenContainer::variablesSize()
		{
			return 0 + AbstractContainer::variablesSize();
		}

	}
}
