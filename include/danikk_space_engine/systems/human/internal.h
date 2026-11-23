#pragma once
#include <danikk_space_engine/world.h>
#include <danikk_space_engine/systems/world_body/world_body.h>
#include <danikk_space_engine/systems/human/human.h>
#include <danikk_space_engine/systems/human/inventory.h>
namespace danikk_space_engine::human_internal
{
	void moveWithGround(World&, WorldBodyId, const WorldBody&, HumanState&,
		glm::dvec3&, const vec3&, float, float);
	void drawEquippedWeapons(const InventoryComponent&, const HumanState&, const mat4&);
}
