#pragma once

#include <cstddef>

#include <danikk_space_engine/fwd.h>
#include <danikk_space_engine/id.h>

namespace danikk_space_engine
{
	/** @brief Stable IDs for registered point-attached component behaviors. */
	enum class PointComponentTypeId : uint32
	{
		none = 0, ///< Empty slot type with no payload callbacks.
		camera, ///< Camera orientation, projection, mouse-look, and movement behavior.
		machine_gun, ///< Weapon model state and timed projectile spawning.
		bullet, ///< Moving projectile state, collision sweep, and tracer rendering.
		engine, ///< Reserved component type; no implementation is registered yet.
		human, ///< Human pose, locomotion, ground following, and appearance.
		inventory, ///< Dynamic item-stack storage attached to a point.
		build_part, ///< Structural cube or slope with rendering and collision geometry.
		damageable, ///< Health and damage resistance for a point-attached object.
		count ///< Number of entries in the point-component type table.
	};

	/** @brief Constructs a component payload in caller-provided storage. */
	using PointComponentConstructFunction = void (*)(void* destination, const void* init_data);
	/** @brief Move-constructs a payload into new storage and leaves the source destructible. */
	using PointComponentMoveFunction = void (*)(void* destination, void* source);
	/** @brief Destroys a payload without releasing the storage owned by its slot. */
	using PointComponentDestroyFunction = void (*)(void* data);
	/** @brief Runs one simulation or presentation callback for a point component slot. */
	using PointComponentUpdateFunction = void (*)(PointComponentSlot& slot, World& world);

		/**
		 * @brief Describes payload layout and callbacks registered for one point-component type.
		 *
		 * The table entry controls how PointData allocates and relocates the payload, and which
		 * callbacks Chunk invokes during simulation ticks and rendered frames. A null callback
		 * means that the type has no behavior for that lifecycle stage.
		 */
	struct PointComponentType
	{
		/** @brief Number of bytes occupied by the concrete component payload. */
		size_t data_size = 0;
		/** @brief Required alignment of the concrete component payload. */
		size_t data_alignment = 1;
		/** @brief Whether the payload can be relocated by copying its inline bytes. */
		bool inline_relocatable = false;
		/** @brief Constructs a payload, optionally copying from type-specific initialization data. */
		PointComponentConstructFunction construct = nullptr;
		/** @brief Relocates a payload when its slot storage changes. */
		PointComponentMoveFunction move = nullptr;
		/** @brief Runs the payload destructor before its storage is reclaimed. */
		PointComponentDestroyFunction destroy = nullptr;
		/** @brief Advances gameplay state; called from the owning chunk's tick traversal. */
		PointComponentUpdateFunction tick = nullptr;
		/** @brief Submits rendering work; called from the owning chunk's frame traversal. */
		PointComponentUpdateFunction frame = nullptr;
	};

	/**
	 * @brief Looks up lifecycle metadata for a point-component type ID.
	 * @param type_id Registered type to look up.
	 * @return The static type-table entry; invalid IDs resolve to the `none` entry.
	 */
	const PointComponentType& getPointComponentType(PointComponentTypeId type_id);

	/**
	 * @brief Stores one component instance attached to a point and its owning chunk.
	 *
	 * Payloads up to `inline_data_capacity` bytes use the inline buffer. Larger or
	 * non-relocatable payloads use `external_data`, whose lifetime is managed through
	 * the registered type callbacks and PointData's component-slot pool.
	 */
	struct PointComponentSlot
	{
		/** @brief Maximum payload size stored directly inside a slot. */
		static constexpr size_t inline_data_capacity = 32;
		/** @brief Owner ID for a slot that has not been attached to a chunk. */
		static constexpr ChunkId no_chunk = static_cast<ChunkId>(-1);

		/** @brief Point whose behavior and data this slot represents. */
		PointId point_id = 0;
		/** @brief Chunk that owns and schedules this component slot. */
		ChunkId chunk_id = no_chunk;
		/** @brief Type-table entry used to interpret the payload and dispatch callbacks. */
		PointComponentTypeId type_id = PointComponentTypeId::none;
		/** @brief Incarnation token used to distinguish reused component slots. */
		uint64 generation_id = 0;
		/** @brief Selects the external pointer instead of the inline byte buffer. */
		bool uses_external_data = false;
		union
		{
			/** @brief Inline storage for small component payloads, aligned for any standard type. */
			alignas(std::max_align_t) uint8 inline_data[inline_data_capacity]{};
			/** @brief Allocated payload storage used when inline storage is unsuitable. */
			void* external_data;
		};

		/**
		 * @brief Returns the active payload address selected by the storage mode.
		 * @return Writable address passed to the component's registered callbacks.
		 */
		void* getData()
		{
			return uses_external_data ? external_data : inline_data;
		}

		/**
		 * @brief Returns the active payload address selected by the storage mode.
		 * @return Read-only address passed to component queries and rendering code.
		 */
		const void* getData() const
		{
			return uses_external_data ? external_data : inline_data;
		}

		/**
		 * @brief Resets ownership, type, storage mode, and inline bytes to the unused-slot defaults.
		 */
		void dispose()
		{
			*this = PointComponentSlot();
		}
	};
}
