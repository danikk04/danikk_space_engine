#pragma once

#include <cstddef>

#include <danikk_space_engine/fwd.h>
#include <danikk_space_engine/id.h>
#include <danikk_space_engine/default.h>

namespace danikk_space_engine
{
	/**
	 * @brief Stable IDs for behaviors attached to a WorldBody.
	 *
	 * The numeric value indexes the static descriptor table in `world_body.cpp`;
	 * `none` is the inert fallback entry and `count` is not a valid component type.
	 */
	enum class WorldBodyComponentTypeId : uint32
	{
		none = 0, ///< Empty type with no payload callbacks.
		planet, ///< Procedural terrain parameters, chunk streaming, edits, and gravity source data.
		count ///< Number of entries in the world-body component type table.
	};

	/**
	 * @brief Constructs a world-body component payload in caller-provided storage.
	 * @param destination Aligned storage selected by the component slot allocator.
	 * @param init_data Optional type-specific initialization data.
	 */
	using WorldBodyComponentConstructFunction = void (*)(void* destination, const void* init_data);
	/**
	 * @brief Move-constructs a payload into replacement storage.
	 * @param destination Uninitialized storage that receives the payload.
	 * @param source Existing payload passed to the type-specific relocation callback.
	 */
	using WorldBodyComponentMoveFunction = void (*)(void* destination, void* source);
	/**
	 * @brief Destroys a payload before its storage is reclaimed.
	 * @param data Address of the live payload.
	 */
	using WorldBodyComponentDestroyFunction = void (*)(void* data);
	/**
	 * @brief Runs one simulation or presentation callback for a world-body component.
	 * @param slot Component instance and payload storage being updated.
	 * @param world World that owns the component and provides access to shared data.
	 */
	using WorldBodyComponentUpdateFunction = void (*)(WorldBodyComponentSlot& slot, World& world);
	/**
	 * @brief Delivers generated chunk data to the component that requested it.
	 * @param slot Component instance receiving the result.
	 * @param world World that validates and dispatches the completed result.
	 * @param generated_chunk Completed worker-produced chunk payload.
	 */
	using WorldBodyComponentChunkFunction = void (*)(
		WorldBodyComponentSlot& slot, World& world, const GeneratedChunkMesh& generated_chunk);

	/**
	 * @brief Describes payload layout and callbacks registered for a world-body component type.
	 *
	 * The descriptor is static registry metadata; instances store only a type ID
	 * and payload. `World::tick()` and `World::frame()` dispatch their respective
	 * callbacks when present. The optional chunk callback handles asynchronous
	 * generation results. Null callbacks mean that the type does not participate
	 * in that stage. Payload storage must satisfy `data_size` and `data_alignment`;
	 * non-inline-relocatable types require a move callback when their slot moves.
	 */
	struct WorldBodyComponentType
	{
		/** @brief Number of bytes occupied by the concrete payload. */
		size_t data_size = 0;
		/** @brief Required alignment of the concrete payload. */
		size_t data_alignment = 1;
		/** @brief Whether the payload can be relocated by copying its inline representation. */
		bool inline_relocatable = false;
		/** @brief Creates the payload from optional component-specific initialization data. */
		WorldBodyComponentConstructFunction construct = nullptr;
		/** @brief Relocates the payload when its pooled slot changes storage. */
		WorldBodyComponentMoveFunction move = nullptr;
		/** @brief Runs the payload destructor before storage is reclaimed. */
		WorldBodyComponentDestroyFunction destroy = nullptr;
		/** @brief Advances component state during World::tick(). */
		WorldBodyComponentUpdateFunction tick = nullptr;
		/** @brief Updates visible component state or submits rendering during World::frame(). */
		WorldBodyComponentUpdateFunction frame = nullptr;
		/** @brief Accepts a completed asynchronous chunk-generation result. */
		WorldBodyComponentChunkFunction accept_generated_chunk = nullptr;
	};

	/**
	 * @brief Looks up lifecycle metadata for a world-body component type ID.
	 * @param type_id Registered type to look up.
	 * @return The static type-table entry; invalid IDs resolve to `none`.
	 */
	const WorldBodyComponentType& getWorldBodyComponentType(WorldBodyComponentTypeId type_id);

	/**
	 * @brief Stores one component instance attached to a WorldBody.
	 *
	 * The slot lives in the WorldBodyData component pool. The owning body's
	 * `component_slot_ids` list supplies membership and iteration, but does not own
	 * the slot allocation. `generation_id` identifies this incarnation so that
	 * asynchronous results for a destroyed or replaced component can be rejected.
	 */
	struct WorldBodyComponentSlot
	{
		/** @brief Maximum payload size stored directly in the slot. */
		static constexpr size_t inline_data_capacity = 32;

		/** @brief Stable pooled ID of this component slot. */
		WorldBodyComponentSlotId slot_id = 0;
		/** @brief World body that owns this component. */
		uint32 world_body_id = 0;
		/** @brief Incarnation token checked when asynchronous work returns. */
		uint64 generation_id = 0;
		/** @brief Type-table entry used to interpret and update the payload. */
		WorldBodyComponentTypeId type_id = WorldBodyComponentTypeId::none;
		/** @brief Selects external allocation instead of the inline buffer. */
		bool uses_external_data = false;
		union
		{
			/** @brief Inline payload storage, aligned for standard object types. */
			alignas(std::max_align_t) uint8 inline_data[inline_data_capacity]{};
			/** @brief Allocated payload storage for large or non-relocatable components. */
			void* external_data;
		};

		/**
		 * @brief Returns the active writable payload address.
		 * @return Storage passed to callbacks registered for `type_id`.
		 */
		void* getData() { return uses_external_data ? external_data : inline_data; }
		/**
		 * @brief Returns the active read-only payload address.
		 * @return Storage used for component queries and rendering.
		 */
		const void* getData() const { return uses_external_data ? external_data : inline_data; }
		/**
		 * @brief Resets the slot fields and inline storage to their unused defaults.
		 *
		 * Call after the live payload has been destroyed and any external allocation
		 * has been released; this function only resets the slot and does not run the
		 * registered destroy callback or free `external_data`.
		 */
		void dispose() { *this = WorldBodyComponentSlot(); }
	};
}
