#include <danikk_space_engine/component/graph.h>

namespace danikk_space_engine
{
	namespace
	{
		void constructGraphMarker(void* destination, const void* init_data)
		{
			*static_cast<uint8*>(destination) = init_data == nullptr
				? 0
				: *static_cast<const uint8*>(init_data);
		}

		void moveGraphMarker(void* destination, void* source)
		{
			*static_cast<uint8*>(destination) = *static_cast<uint8*>(source);
		}

		void destroyGraphMarker(void*) {}

		const GraphComponentType graph_component_types[] =
		{
			{}, // none
			{
				sizeof(uint8),
				alignof(uint8),
				true,
				constructGraphMarker,
				moveGraphMarker,
				destroyGraphMarker,
				nullptr,
				nullptr
			}, // pipe
			{
				sizeof(uint8),
				alignof(uint8),
				true,
				constructGraphMarker,
				moveGraphMarker,
				destroyGraphMarker,
				nullptr,
				nullptr
			} // wire
		};
	}

	const GraphComponentType& getGraphComponentType(GraphComponentTypeId type_id)
	{
		const uint32 index = static_cast<uint32>(type_id);
		if(index >= static_cast<uint32>(GraphComponentTypeId::count))
		{
			return graph_component_types[static_cast<uint32>(GraphComponentTypeId::none)];
		}
		return graph_component_types[index];
	}
}
