#include <danikk_space_engine/component/graph/graph.h>

namespace danikk_space_engine
{
	namespace
	{

		/**
		 * @brief Initializes the one-byte placeholder payload used by pipe and wire registrations.
		 * @param destination Storage for the marker byte.
		 * @param init_data Optional byte copied into the marker; null initializes it to zero.
		 */
		void constructGraphMarker(void* destination, const void* init_data)
		{
			*static_cast<uint8*>(destination) = init_data == nullptr
				? 0
				: *static_cast<const uint8*>(init_data);
		}

		/**
		 * @brief Copies the trivial marker byte when a graph slot is relocated.
		 * @param destination Replacement payload storage.
		 * @param source Existing marker payload.
		 */
		void moveGraphMarker(void* destination, void* source)
		{
			*static_cast<uint8*>(destination) = *static_cast<uint8*>(source);
		}

		/** @brief No-op destructor for the trivially destructible marker payload. */
		void destroyGraphMarker(void*) {}

		/**
		 * @brief Static callback table indexed by GraphComponentTypeId.
		 *
		 * The reserved `none` row is empty. Pipe and wire currently allocate a one-byte marker
		 * payload with trivial relocation and destruction, but have no simulation or rendering
		 * callback implementation yet.
		 */
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

	/**
	 * @brief Resolves a graph-component ID to its static storage and callback descriptor.
	 * @param type_id Component type requested by a graph slot or allocator.
	 * @return Registered descriptor, or the inert `none` descriptor for an out-of-range ID.
	 */
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
