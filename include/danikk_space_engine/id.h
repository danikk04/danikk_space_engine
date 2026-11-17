#pragma once

#include <danikk_space_engine/default.h>

namespace danikk_space_engine
{
	/** @brief 32-bit handle for an entry in WorldData's global point pool. */
	using PointId = uint32;
	/** @brief 32-bit handle for a spatial chunk owned by a WorldBody. */
	using ChunkId = uint32;
	/** @brief 32-bit handle for a world-space body and its transform. */
	using WorldBodyId = uint32;
	/** @brief 32-bit handle for a ground triangle stored in WorldData. */
	using GroundTriangleId = uint32;
	/** @brief 32-bit index identifying a material definition. */
	using MaterialId = uint32;
	/** @brief 32-bit index identifying a registered shape definition. */
	using ShapeId = uint32;
	/** @brief 32-bit index identifying a planet settings preset. */
	using PlanetSettingsId = uint32;
	/** @brief 32-bit index into the shared Minecraft skin texture catalog. */
	using HumanSkinId = uint32;
	/** @brief 32-bit index into the item-category behavior table. */
	using ItemTypeId = uint32;
	/** @brief 32-bit index into the concrete item-definition catalog. */
	using ItemDefinitionId = uint32;
	/** @brief 32-bit index into the buildable-detail catalog. */
	using BuildDefinitionId = uint32;

	/** @brief 32-bit handle for a point-component slot in WorldData. */
	using PointComponentSlotId = uint32;
	/** @brief 32-bit handle for a graph-component slot in WorldData. */
	using GraphComponentSlotId = uint32;
	/** @brief 32-bit handle for a WorldBody-component slot in WorldData. */
	using WorldBodyComponentSlotId = uint32;
}
