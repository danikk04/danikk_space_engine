#pragma once

#include <cstddef>

#include <danikk_space_engine/fwd.h>
#include <danikk_space_engine/id.h>
#include <danikk_space_engine/default.h>

namespace danikk_space_engine
{
	enum class WorldBodyComponentTypeId : uint32
	{
		none = 0,
		planet,
		count
	};

	using WorldBodyComponentConstructFunction = void (*)(void* destination, const void* init_data);
	using WorldBodyComponentMoveFunction = void (*)(void* destination, void* source);
	using WorldBodyComponentDestroyFunction = void (*)(void* data);
	using WorldBodyComponentUpdateFunction = void (*)(WorldBodyComponentSlot& slot, World& world);
	using WorldBodyComponentChunkFunction = void (*)(
		WorldBodyComponentSlot& slot, World& world, const GeneratedChunkData& generated_chunk);

	struct WorldBodyComponentType
	{
		size_t data_size = 0;
		size_t data_alignment = 1;
		bool inline_relocatable = false;
		WorldBodyComponentConstructFunction construct = nullptr;
		WorldBodyComponentMoveFunction move = nullptr;
		WorldBodyComponentDestroyFunction destroy = nullptr;
		WorldBodyComponentUpdateFunction tick = nullptr;
		WorldBodyComponentUpdateFunction frame = nullptr;
		WorldBodyComponentChunkFunction accept_generated_chunk = nullptr;
	};

	const WorldBodyComponentType& getWorldBodyComponentType(WorldBodyComponentTypeId type_id);

	struct WorldBodyComponentSlot
	{
		static constexpr size_t inline_data_capacity = 32;

		WorldBodyComponentSlotId slot_id = 0;
		uint32 world_body_id = 0;
		uint64 generation_id = 0;
		WorldBodyComponentTypeId type_id = WorldBodyComponentTypeId::none;
		bool uses_external_data = false;
		union
		{
			alignas(std::max_align_t) uint8 inline_data[inline_data_capacity]{};
			void* external_data;
		};

		void* getData() { return uses_external_data ? external_data : inline_data; }
		const void* getData() const { return uses_external_data ? external_data : inline_data; }
		void dispose() { *this = WorldBodyComponentSlot(); }
	};
}
