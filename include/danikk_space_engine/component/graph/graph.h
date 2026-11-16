#pragma once

#include <cstddef>

#include <danikk_space_engine/fwd.h>
#include <danikk_space_engine/id.h>
#include <danikk_space_engine/world/point.h>

namespace danikk_space_engine
{
	/**
	 * @brief Stable IDs for registered edge-attached component behaviors.
	 *
	 * `pipe` and `wire` currently reserve catalog entries with a marker-byte payload but no tick
	 * or frame callbacks. `count` sizes the callback table and is not a usable component type.
	 */
	enum class GraphComponentTypeId : uint32
	{
		none = 0, ///< Empty type with no payload callbacks.
		pipe, ///< Reserved placeholder for a pipe edge; current payload has no simulation behavior.
		wire, ///< Reserved placeholder for a wire edge; current payload has no simulation behavior.
		count ///< Number of entries in the graph-component type table.
	};

	/** @brief Constructs a graph-component payload in caller-provided storage. */
	using GraphComponentConstructFunction = void (*)(void* destination, const void* init_data);
	/** @brief Move-constructs a graph-component payload into replacement storage. */
	using GraphComponentMoveFunction = void (*)(void* destination, void* source);
	/** @brief Destroys a graph-component payload before its storage is reclaimed. */
	using GraphComponentDestroyFunction = void (*)(void* data);
	/** @brief Runs one simulation or presentation callback for a graph-component slot. */
	using GraphComponentUpdateFunction = void (*)(GraphComponentSlot& slot, World& world);

	/**
	 * @brief Describes payload layout and callbacks registered for one graph-component type.
	 *
	 * Graph components represent behavior attached to an edge between two points. Their
	 * type-table entry governs payload allocation and optional tick/frame dispatch by Chunk.
	 * The `none` entry is inert and cannot be used to construct a slot through WorldData.
	 */
	struct GraphComponentType
	{
		/** @brief Number of bytes occupied by the concrete payload. */
		size_t data_size = 0;
		/** @brief Required alignment of the concrete payload. */
		size_t data_alignment = 1;
		/** @brief Whether the payload can be relocated by copying its inline representation. */
		bool inline_relocatable = false;
		/** @brief Creates a payload from optional type-specific initialization data. */
		GraphComponentConstructFunction construct = nullptr;
		/** @brief Relocates a payload when its slot storage changes. */
		GraphComponentMoveFunction move = nullptr;
		/** @brief Runs the payload destructor. */
		GraphComponentDestroyFunction destroy = nullptr;
		/** @brief Advances the edge behavior during the owning chunk's simulation tick. */
		GraphComponentUpdateFunction tick = nullptr;
		/** @brief Submits edge rendering during the owning chunk's frame traversal. */
		GraphComponentUpdateFunction frame = nullptr;
	};

	/**
	 * @brief Looks up lifecycle metadata for a graph-component type ID.
	 * @param type_id Registered type to look up.
	 * @return The static type-table entry; invalid IDs resolve to `none`.
	 */
	const GraphComponentType& getGraphComponentType(GraphComponentTypeId type_id);

	/**
	 * @brief Stores one behavior instance attached to the edge joining two world points.
	 *
	 * Small relocatable payloads up to 32 bytes are stored inline; larger or non-relocatable data
	 * uses an external allocation. The slot itself is pooled by WorldData and listed by its chunk,
	 * which schedules its callbacks. The endpoints are pooled Point IDs; callers must keep both
	 * points active for the lifetime of the component. `dispose()` is only for a payload that has
	 * already been destroyed and any external allocation freed.
	 */
	struct GraphComponentSlot
	{
		/** @brief Maximum payload size stored in the slot's inline buffer. */
		static constexpr size_t inline_data_capacity = 32;

		/** @brief First endpoint Point ID of the graph edge. */
		PointId point_a = 0;
		/** @brief Second endpoint Point ID of the graph edge. */
		PointId point_b = 0;
		/** @brief Type-table entry used to interpret the payload and dispatch callbacks. */
		GraphComponentTypeId type_id = GraphComponentTypeId::none;
		/** @brief Selects `external_data` instead of the inline storage buffer. */
		bool uses_external_data = false;
		union
		{
			/** @brief Inline storage for small payloads, aligned for standard object types. */
			alignas(std::max_align_t) uint8 inline_data[inline_data_capacity]{};
			/** @brief Allocated storage for payloads that do not fit inline or cannot relocate inline. */
			void* external_data;
		};

		/**
		 * @brief Returns the active writable payload address.
		 * @return Storage passed to the callbacks registered for `type_id`.
		 */
		void* getData()
		{
			return uses_external_data ? external_data : inline_data;
		}

		/**
		 * @brief Returns the active read-only payload address.
		 * @return Storage used by component queries and rendering code.
		 */
		const void* getData() const
		{
			return uses_external_data ? external_data : inline_data;
		}

		/**
		 * @brief Resets endpoints, type, storage mode, and inline bytes to unused-slot defaults.
		 */
		void dispose()
		{
			*this = GraphComponentSlot();
		}
	};
}
