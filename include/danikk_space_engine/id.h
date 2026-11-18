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
	/** @brief 32-bit index identifying a material definition. */
	using MaterialId = uint32;
	/** @brief 32-bit index identifying a planet settings preset. */
	using PlanetSettingsId = uint32;
	/** @brief 32-bit index into the shared Minecraft skin texture catalog. */
	using HumanSkinId = uint32;
	/** @brief 32-bit index into the item-category definition table. */
	using ItemCategoryId = uint32;
	/** @brief 32-bit index into the concrete item-definition catalog. */
	using ItemDefinitionId = uint32;
	/** @brief Index of a projectile definition. */
	using ProjectileDefinitionId = uint32;
	/** @brief 32-bit index into the selectable tool-command catalog. */
	using ToolCatalogEntryId = uint32;
	/** @brief 32-bit index identifying a category in the tool-command catalog. */
	using ToolCategoryId = uint32;
	/** @brief Index of a faction in the current world. */
	using FactionId = uint32;
	/** @brief Index of a built-in faction template. */
	using FactionDefinitionId = uint32;
	/** @brief Index into the damageable-object definition table. */
	using DamageableDefinitionId = uint32;

	/** @brief 32-bit handle for a point-component slot in WorldData. */
	using PointComponentSlotId = uint32;
	/** @brief 32-bit handle for a graph-component slot in WorldData. */
	using GraphComponentSlotId = uint32;
	/** @brief 32-bit handle for a WorldBody-component slot in WorldData. */
	using WorldBodyComponentSlotId = uint32;
}
